// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#include "scanner.h"

#include "indexer.h"
#include "indexingresultqueue.h"
#include "workqueue.h"

#include "frontendactionfactory.h"
#include "fileidentificator.h"
#include "fileindexingarbiter.h"
#include "translationunitindex.h"

#include "cppscanner/snapshot/merge.h"

#include "cppscanner/database/transaction.h"

#include "cppscanner/base/glob.h"
#include "cppscanner/base/os.h"
#include "cppscanner/base/version.h"

#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/JSONCompilationDatabase.h>

#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/FileManager.h>
#include <clang/Basic/LLVM.h>
#include <clang/Driver/Compilation.h>
#include <clang/Driver/Driver.h>
#include <clang/Driver/Job.h>
#include <clang/Driver/Options.h>
#include <clang/Driver/Tool.h>
#include <clang/Driver/ToolChain.h>

#include <clang/Frontend/CompilerInvocation.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>

#include <llvm/TargetParser/Host.h>

#include <algorithm>
#include <deque>
#include <fstream>
#include <iostream>
#include <optional>
#include <thread>
#include <vector>

namespace cppscanner
{

struct ScannerCompileCommand : CompileCommand
{
  bool translated = false;
  std::optional<std::filesystem::path> pchOutput;
  std::optional<std::filesystem::path> pcmOutput;
};

struct ScannerData
{
  std::string homeDirectory;
  std::optional<std::string> rootDirectory;
  bool indexExternalFiles = false;
  bool indexLocalSymbols = false;
  size_t nbThreads = 0;
  std::vector<std::string> filters;
  std::vector<std::string> translationUnitFilters;
  bool captureFileContent = true;
  bool remapFileIds = false;
  Snapshot::Properties extraSnapshotProperties;

  std::filesystem::path outputPath;

  std::vector<ScannerCompileCommand> compileCommands;
  bool forceStripOutput = false;

  std::unique_ptr<FileIdentificator> fileIdentificator;
  std::vector<bool> filePathsInserted;
  std::vector<bool> indexedFiles;
  std::map<SymbolID, IndexerSymbol> symbols;
};

static std::unique_ptr<FileIndexingArbiter> createIndexingArbiter(ScannerData& d)
{
  CreateIndexingArbiterOptions opts;
  opts.filters = d.filters;
  opts.homeDirectory = d.homeDirectory;
  opts.rootDirectory = d.rootDirectory.value_or(std::string());
  opts.indexExternalFiles = d.indexExternalFiles;
  return createIndexingArbiter(*d.fileIdentificator, opts);
}

class ThreadProcIndexDataConsumer : public ForwardingIndexDataConsumer
{
private:
  IndexingResultQueue& m_resultsQueue;
  bool m_produced_output = false;

public:
  ThreadProcIndexDataConsumer(Indexer& indexer, IndexingResultQueue& resultsQueue) : ForwardingIndexDataConsumer(&indexer),
    m_resultsQueue(resultsQueue)
  {

  }

  bool didProduceOutput() const
  {
    return m_produced_output;
  }

  void resetDidProduceOutput()
  {
    m_produced_output = false;
  }

protected:
  void initialize(clang::ASTContext& ctx) final
  {
    ForwardingIndexDataConsumer::initialize(ctx);

    resetDidProduceOutput();
  }

  void finish() final
  {
    ForwardingIndexDataConsumer::finish();

    m_resultsQueue.write(std::move(*indexer().getCurrentIndex()));
    indexer().resetCurrentIndex();
    m_produced_output = true;
  }

};

Scanner::Scanner()
{
  d = std::make_unique<ScannerData>();
  d->homeDirectory = std::filesystem::current_path().generic_u8string();
}

Scanner::~Scanner() = default;

void Scanner::setOutputPath(const std::filesystem::path& p)
{
  d->outputPath = p;
}

void Scanner::setHomeDir(const std::filesystem::path& p)
{
  d->homeDirectory = std::filesystem::absolute(p).generic_u8string();
}

void Scanner::setRootDir(const std::filesystem::path& p)
{
  d->rootDirectory = std::filesystem::absolute(p).generic_u8string();
}

void Scanner::setIndexExternalFiles(bool on)
{
  d->indexExternalFiles = on;
}

void Scanner::setIndexLocalSymbols(bool on)
{
  d->indexLocalSymbols = on;
}

void Scanner::setFilters(const std::vector<std::string>& filters)
{
  d->filters = filters;
}

void Scanner::setTranslationUnitFilters(const std::vector<std::string>& filters)
{
  d->translationUnitFilters = filters;
}

void Scanner::setNumberOfParsingThread(size_t n)
{
  d->nbThreads = n;
}

void Scanner::setCaptureFileContent(bool on)
{
  d->captureFileContent = on;
}

void Scanner::setRemapFileIds(bool on)
{
  d->remapFileIds = on;
}

void Scanner::initSnapshot()
{
  assert(!d->outputPath.empty());
  if (d->outputPath.empty())
  {
    throw std::runtime_error("missing output path");
  }

  const std::filesystem::path dbPath = d->remapFileIds ? 
    std::filesystem::path(d->outputPath.generic_u8string() + ".tmp") : d->outputPath;

  if (d->remapFileIds && std::filesystem::exists(dbPath))
  {
    // this shouldn't happen, unless the scanner crashed during a previous execution.
    std::filesystem::remove(dbPath);
  }

  assert(d->fileIdentificator);
  m_snapshot_creator = std::make_unique<SnapshotCreator>(*d->fileIdentificator);

  m_snapshot_creator->setHomeDir(d->homeDirectory);
  m_snapshot_creator->setCaptureFileContent(d->captureFileContent);

  m_snapshot_creator->init(dbPath);

  // TODO: revoir le passage de la valeur
  m_snapshot_creator->writeProperty("scanner.indexExternalFiles", d->indexExternalFiles ? "true" : "false");
  m_snapshot_creator->writeProperty("scanner.indexLocalSymbols", d->indexLocalSymbols ? "true" : "false");
  m_snapshot_creator->writeProperty("scanner.root", Snapshot::Path(d->rootDirectory.value_or(std::string())).str());
  m_snapshot_creator->writeProperty("scanner.workingDirectory", Snapshot::Path(std::filesystem::current_path().generic_u8string()).str());

  for (const auto& p : d->extraSnapshotProperties)
  {
    m_snapshot_creator->writeProperty(p.first, p.second);
  }
}

void Scanner::setExtraProperty(const std::string& name, const std::string& value)
{
  d->extraSnapshotProperties[name] = value;
}

SnapshotCreator* Scanner::snapshotCreator() const
{
  return m_snapshot_creator.get();
}

bool Scanner::passTranslationUnitFilters(const std::string& filename) const
{
  if (!d->translationUnitFilters.empty())
  {
    bool exclude = std::none_of(d->translationUnitFilters.begin(), d->translationUnitFilters.end(), [&filename](const std::string& e) {
      return is_glob_pattern(e) ? glob_match(filename, e) : filename_match(filename, e);
      });

    return !exclude;
  }

  return true;
}

static void removeGmArg(std::vector<std::string>& commandLine)
{
  auto it = std::find(commandLine.begin(), commandLine.end(), "/Gm-");

  if (it != commandLine.end())
  {
    commandLine.erase(it);
  }
}

void Scanner::scanFromCompileCommands(const std::filesystem::path& compileCommandsPath)
{
  std::string error_message;
  auto compile_commands = clang::tooling::JSONCompilationDatabase::loadFromFile(
    compileCommandsPath.u8string().c_str(), std::ref(error_message), clang::tooling::JSONCommandLineSyntax::AutoDetect
  );

  if (!compile_commands) {
    std::cerr << "error while parsing compile_commands.json file: " << error_message << std::endl;
    return;
  }

  std::cout << "Processing compile_commands.json..." << std::endl;

  for (const clang::tooling::CompileCommand& cc : compile_commands->getAllCompileCommands())
  {
    ScannerCompileCommand scanner_command;
    scanner_command.fileName = cc.Filename;
    scanner_command.commandLine = cc.CommandLine;
    d->compileCommands.push_back(std::move(scanner_command));
  }

  std::cout << "Found " << d->compileCommands.size() << " translation units." << std::endl;

  runScanSingleOrMultiThreaded();
}

inline const char* getDefaultCompilerExecutableName()
{
#ifdef _WIN32
  return "clang++";
#else
  return "/usr/bin/c++";
#endif
}

void Scanner::scanFromListOfInputs(const std::vector<std::filesystem::path>& inputs, const std::vector<std::string>& compileArgs)
{
  // TODO: add support for glob pattern
  auto queue = std::deque<std::filesystem::path>(inputs.begin(), inputs.end());

  while (!queue.empty())
  {
    std::filesystem::path item = queue.front();
    queue.pop_front();

    const auto input = std::filesystem::absolute(item);

    if (!std::filesystem::is_regular_file(input))
    {
      if (std::filesystem::is_directory(input))
      {
        for (const auto& dir_entry : std::filesystem::directory_iterator(input))
        {
          if (dir_entry.is_directory())
          {
            queue.push_back(dir_entry.path());
          }
          else if (dir_entry.is_regular_file())
          {
            if (dir_entry.path().extension().generic_u8string() == ".cpp")
            {
              queue.push_back(dir_entry.path());
            }
          }
        }
      }

      continue;
    }

    ScannerCompileCommand scanner_command;
    scanner_command.fileName = input.generic_u8string();

    scanner_command.commandLine = { getDefaultCompilerExecutableName() };
    scanner_command.commandLine.insert(scanner_command.commandLine.end(), compileArgs.begin(), compileArgs.end());
    scanner_command.commandLine.insert(scanner_command.commandLine.end(), scanner_command.fileName);
   
    d->compileCommands.push_back(std::move(scanner_command));
  }

  if (d->compileCommands.size() > 1)
  {
    std::cout << "Found " << d->compileCommands.size() << " translation units." << std::endl;
  }

  d->forceStripOutput = true;

  runScanSingleOrMultiThreaded();
}

void Scanner::scan(const std::vector<CompileCommand>& compileCommands)
{
  d->compileCommands.reserve(compileCommands.size());

  for (const CompileCommand& cc : compileCommands)
  {
    ScannerCompileCommand scc;
    scc.fileName = cc.fileName;
    scc.commandLine = cc.commandLine;
    d->compileCommands.push_back(std::move(scc));
  }

  runScanSingleOrMultiThreaded();
}

static std::optional<std::filesystem::path> findOutput(const CompileCommand& cc)
{
  auto it = std::find(cc.commandLine.begin(), cc.commandLine.end(), "-o");

  if (it == cc.commandLine.end()) {
    return std::nullopt;
  }

  return std::filesystem::path(*std::next(it));
}


class CCAdjuster
{
private:
  bool m_forceStripOutput;

public:
  explicit CCAdjuster(bool forceStripOutput) 
    : m_forceStripOutput(forceStripOutput)
  {

  }

  void adjustCompileCommands(std::vector<ScannerCompileCommand>& commands)
  {
    clang::tooling::ArgumentsAdjuster soAdjuster = getSyntaxOnlyAdjuster();

    if (m_forceStripOutput)
    {
      for (ScannerCompileCommand& cc : commands)
      {
        cc.commandLine = soAdjuster(cc.commandLine, cc.fileName);
      }

      return;
    }

    clang::tooling::ArgumentsAdjuster ppAdjuster = getPreprocessingRecordAdjuster();

    for (ScannerCompileCommand& cc : commands)
    {
      cc.pchOutput = findPchOutput(cc);

      if (!cc.pchOutput.has_value())
      {
        cc.pcmOutput = findPcmOutput(cc);
      }

      if (!cc.pchOutput.has_value() && !cc.pcmOutput.has_value())
      {
        cc.commandLine = soAdjuster(cc.commandLine, cc.fileName);
      }
      else
      {
        cc.commandLine = ppAdjuster(cc.commandLine, cc.fileName);
      }
    }
  }

private:

  static clang::tooling::ArgumentsAdjuster getSyntaxOnlyAdjuster()
  {
    clang::tooling::ArgumentsAdjuster syntaxOnly = clang::tooling::getClangSyntaxOnlyAdjuster();
    clang::tooling::ArgumentsAdjuster stripOutput = clang::tooling::getClangStripOutputAdjuster();
    clang::tooling::ArgumentsAdjuster detailedPreprocRecord = clang::tooling::getInsertArgumentAdjuster({ "-detailed-preprocessing-record" }, clang::tooling::ArgumentInsertPosition::END);
    return clang::tooling::combineAdjusters(clang::tooling::combineAdjusters(syntaxOnly, stripOutput), detailedPreprocRecord);
  }

  static clang::tooling::ArgumentsAdjuster getPreprocessingRecordAdjuster()
  {
    clang::tooling::ArgumentsAdjuster detailedPreprocRecord = clang::tooling::getInsertArgumentAdjuster({ "-detailed-preprocessing-record" }, clang::tooling::ArgumentInsertPosition::END);
    return detailedPreprocRecord;
  }

  static std::optional<std::filesystem::path> findPchOutput(const CompileCommand& cc)
  {
    auto it = std::find(cc.commandLine.begin(), cc.commandLine.end(), "-emit-pch");

    if (it == cc.commandLine.end()) {
      return std::nullopt;
    }

    return findOutput(cc);
  }

  static std::optional<std::filesystem::path> findPcmOutput(const CompileCommand& cc)
  {
    auto it = std::find(cc.commandLine.begin(), cc.commandLine.end(), "-emit-module-interface");

    if (it == cc.commandLine.end()) {
      return std::nullopt;
    }

    return findOutput(cc);
  }
};

// converts user-toolchain command line args to clang cc1.
// code largely stolen from clang/Tooling/Tooling.cpp, with 
// some minor adaptations.
class CCTranslator
{
private:
  clang::IntrusiveRefCntPtr<clang::FileManager> m_files;
  clang::CompilerInstance m_ci;

public:
  CCTranslator()
  {
    m_files = clang::IntrusiveRefCntPtr<clang::FileManager>(new clang::FileManager(clang::FileSystemOptions()));
    m_ci.createDiagnostics();
  }

  explicit CCTranslator(clang::FileManager& files) :
    m_files(&files)
  {
    // TODO: add an ignoring diagnostic consumer to the diagnostic engine?
    m_ci.createDiagnostics();
  }

  void translateCommands(std::vector<ScannerCompileCommand>& commands)
  {
    for (ScannerCompileCommand& cc : commands)
    {
#if _WIN32
      removeGmArg(cc.commandLine);
#endif

      translate(cc);
    }

    // remove failed translations
    {
      auto it = std::remove_if(commands.begin(), commands.end(), [](const ScannerCompileCommand& cc) {
        return !cc.translated;
        });

      if (it != commands.end())
      {
        std::cout << "Some commands could not be translated and will be ignored." << std::endl;
        commands.erase(it, commands.end());
      }
    }
  }

  clang::FileManager* fileManager() const
  {
    return m_files.get();
  }

protected:
  void translate(ScannerCompileCommand& cc)
  {
    const char* BinaryName = cc.commandLine.front().c_str();

    const auto Driver = newDriver(m_ci.getDiagnostics(), BinaryName, &m_files->getVirtualFileSystem());

    // The "input file not found" diagnostics from the driver are useful.
    // The driver is only aware of the VFS working directory, but some clients
    // change this at the FileManager level instead.
    // In this case the checks have false positives, so skip them.
    if (!m_files->getFileSystemOpts().WorkingDir.empty())
      Driver->setCheckInputsExist(false);

    std::vector<const char*> argv;
    argv.reserve(cc.commandLine.size());
    for (const std::string& a : cc.commandLine)
    {
      argv.push_back(a.c_str());
    }

    const std::unique_ptr<clang::driver::Compilation> Compilation{
      Driver->BuildCompilation(argv)
    };

    if (!Compilation)
      return;

    cc.commandLine = getCC1Arguments(*Compilation);
    cc.translated = !cc.commandLine.empty();
  }

private:
  static std::unique_ptr<clang::driver::Driver> newDriver(clang::DiagnosticsEngine& diagnostics, const char *BinaryName,
    clang::IntrusiveRefCntPtr<llvm::vfs::FileSystem> VFS) {
    auto CompilerDriver =
      std::make_unique<clang::driver::Driver>(BinaryName, llvm::sys::getDefaultTargetTriple(),
        diagnostics, "clang LLVM compiler", std::move(VFS));
    CompilerDriver->setTitle("clang_based_tool");
    return CompilerDriver;
  }


  std::vector<std::string> getCC1Arguments(clang::driver::Compilation& Compilation) {
    const clang::driver::JobList &Jobs = Compilation.getJobs();
    auto IsCC1Command = [](const clang::driver::Command &Cmd) {
      return llvm::StringRef(Cmd.getCreator().getName()) == "clang";
      };
    auto IsSrcFile = [](const clang::driver::InputInfo &II) {
      return clang::driver::types::isSrcFile(
        II.getType());
      };
    llvm::SmallVector<const clang::driver::Command *, 1> CC1Jobs;
    for (const clang::driver::Command &Job : Jobs)
      if (IsCC1Command(Job) && llvm::all_of(Job.getInputInfos(), 
        IsSrcFile))
        CC1Jobs.push_back(
          &Job);
    // If there are no jobs for source files, try checking again for a single job
    // with any file type. This accepts a preprocessed file as input.
    if (CC1Jobs.empty())
    {
      for (const clang::driver::Command& Job : Jobs)
      {
        if (IsCC1Command(Job)) {
          CC1Jobs.push_back(&Job);
        }
      }
    }

    if (CC1Jobs.empty()) {
      return {};
    }

    const llvm::opt::ArgStringList& args = CC1Jobs[0]->getArguments();
    std::vector<std::string> result;
    result.reserve(args.size() + 1);
    result.push_back("clang");
    result.insert(result.end(), args.begin(), args.end());
    return result;
  }
};

static void translateAndAdjust(std::vector<ScannerCompileCommand>& commands, CCTranslator& translator, bool forceStripOutput)
{
  translator.translateCommands(commands);

  // adjust compile commands
  {
    CCAdjuster adjuster{ forceStripOutput };
    adjuster.adjustCompileCommands(commands);
  }
}

static void compilePCH(const CompileCommand& cc, const std::filesystem::path& pchOutput, clang::FileManager* fileManager)
{
  if (!pchOutput.parent_path().empty())
  {
    std::filesystem::create_directories(pchOutput.parent_path());
  }
  auto action = std::make_unique<clang::GeneratePCHAction>();
  clang::tooling::ToolInvocation invocation{ cc.commandLine, std::move(action), fileManager};
  const bool success = invocation.run();
  if (!success)
  {
    std::cerr << "pch compilation failed: " << cc.fileName << std::endl;
  }
}

static void compilePCM(const CompileCommand& cc, const std::filesystem::path& pcmOutput, clang::FileManager* fileManager)
{
  if (!pcmOutput.parent_path().empty())
  {
    std::filesystem::create_directories(pcmOutput.parent_path());
  }

  auto action = std::make_unique<clang::GenerateModuleInterfaceAction>();
  clang::tooling::ToolInvocation invocation{ cc.commandLine, std::move(action), fileManager};
  const bool success = invocation.run();
  if (!success)
  {
    std::cerr << "pcm compilation failed: " << cc.fileName << std::endl;
  }
}

void Scanner::scanSingleThreaded()
{
  assert(d->fileIdentificator);
  std::unique_ptr<FileIndexingArbiter> indexing_arbiter = createIndexingArbiter(*d);
  clang::IntrusiveRefCntPtr<clang::FileManager> file_manager{ new clang::FileManager(clang::FileSystemOptions()) };

  CCTranslator translator{ *file_manager };
  translateAndAdjust(d->compileCommands, translator, d->forceStripOutput);

  processCommands(d->compileCommands, *indexing_arbiter, *file_manager);
}

static bool run_invocation(const WorkQueue::ToolInvocation& invocation, Indexer& indexer, IndexingFrontendActionFactory& actionfactory, clang::FileManager* fileManager)
{
  if (!std::filesystem::exists(invocation.filename))
  {
    return false;
  }

  clang::tooling::ToolInvocation tool_invocation{ invocation.command, actionfactory.create(), fileManager };
  tool_invocation.setDiagnosticConsumer(indexer.getOrCreateDiagnosticConsumer());
  return tool_invocation.run();
}

void parsing_thread_proc(ScannerData* data, FileIndexingArbiter* arbiter, WorkQueue* inputQueue, IndexingResultQueue* resultQueue, std::atomic<int>& running)
{
  clang::IntrusiveRefCntPtr<clang::FileManager> file_manager{ new clang::FileManager(clang::FileSystemOptions()) };
  Indexer indexer{ *arbiter };
  auto index_data_consumer = std::make_shared<ThreadProcIndexDataConsumer>(indexer, *resultQueue);
  IndexingFrontendActionFactory actionfactory{ index_data_consumer };
  actionfactory.setIndexLocalSymbols(data->indexLocalSymbols);

  for (;;)
  {
    std::optional<WorkQueue::ToolInvocation> item = inputQueue->next();

    if (!item.has_value())
    {
      break;
    }

    bool success = false;

    try {
      success = run_invocation(*item, indexer, actionfactory, file_manager.get());
    }
    catch (...)
    {
      success = false;
    }

    if (!success || !index_data_consumer->didProduceOutput())
    {
      TranslationUnitIndex index;
      index.mainFileId = arbiter->fileIdentificator().getIdentification(item->filename);
      index.isError = true;
      resultQueue->write(std::move(index));
    }

    index_data_consumer->resetDidProduceOutput();
  }

  running.fetch_sub(1);
}

class WorkerThreads
{
private:
  ScannerData* m_data; FileIndexingArbiter* m_arbiter; WorkQueue* m_inputQueue; IndexingResultQueue* m_resultQueue;
  std::vector<std::thread> m_threads;
  std::atomic<int> m_running = 0;

public:

  WorkerThreads(ScannerData* data, FileIndexingArbiter* arbiter, WorkQueue* inputQueue, IndexingResultQueue* resultQueue) :
    m_data(data), m_arbiter(arbiter), m_inputQueue(inputQueue), m_resultQueue(resultQueue)
  {

  }

  ~WorkerThreads()
  {
    destroy();
  }

  void addOne()
  {
    m_running.fetch_add(1);

    std::thread worker{ parsing_thread_proc, m_data, m_arbiter, m_inputQueue, m_resultQueue, std::ref(m_running) };
    m_threads.emplace_back(std::move(worker));
  }

  void add(size_t n)
  {
    while (n-- > 0) {
      addOne();
    }
  }

  int nbRunning()
  {
    return m_running.load();
  }

  void destroy()
  {
    for (std::thread& worker : m_threads)
    {
      worker.join();
    }
    m_threads.clear();
  }
};

void Scanner::scanMultiThreaded()
{
  assert(d->nbThreads > 0);
  assert(d->fileIdentificator);

  std::unique_ptr<FileIndexingArbiter> indexing_arbiter = createIndexingArbiter(*d);

  if (d->nbThreads > 1)
  {
    indexing_arbiter = FileIndexingArbiter::createThreadSafeArbiter(std::move(indexing_arbiter));
  }

  CCTranslator translator;
  translateAndAdjust(d->compileCommands, translator, d->forceStripOutput);

  std::vector<WorkQueue::ToolInvocation> tasks;
  std::vector<ScannerCompileCommand> pch_ccs;

  for (ScannerCompileCommand& cc : d->compileCommands)
  {
    const std::optional<std::filesystem::path>& pch_output = cc.pchOutput;
    const std::optional<std::filesystem::path>& pcm_output = cc.pcmOutput;

    if (pch_output.has_value() || pcm_output.has_value())
    {
      pch_ccs.push_back(cc);
    }
    else
    {
      if (!passTranslationUnitFilters(cc.fileName)) {
        std::cout << "[SKIPPED] " << cc.fileName << std::endl;
        continue;
      }

      tasks.push_back(WorkQueue::ToolInvocation{ cc.fileName, cc.commandLine });
    }
  }

  if (!pch_ccs.empty())
  {
    std::cout << "Generating precompiled headers..." << std::endl;
    processCommands(pch_ccs, *indexing_arbiter, *translator.fileManager());
  }

  WorkQueue input_queue{ tasks };
  IndexingResultQueue results_queue;

  WorkerThreads threads{ d.get(), indexing_arbiter.get(), &input_queue, &results_queue };
  threads.add(d->nbThreads);

  while (threads.nbRunning() > 0)
  {
    std::optional<TranslationUnitIndex> item = results_queue.tryRead(std::chrono::milliseconds(250));

    if (!item.has_value())
      continue;

    TranslationUnitIndex& result = item.value();
    if (!result.isError)
    {
      m_snapshot_creator->feed(std::move(result));
    }
    else 
    {
      std::cout << "error: tool invocation failed for " << d->fileIdentificator->getFile(result.mainFileId) << std::endl;
    }
  }

  threads.destroy();
}

void Scanner::runScanSingleOrMultiThreaded()
{
  const bool single_threaded = d->nbThreads == 0;

  if (single_threaded)
  {
    d->fileIdentificator = FileIdentificator::createFileIdentificator();
  }
  else
  {
    d->fileIdentificator = FileIdentificator::createThreadSafeFileIdentificator();
  }

  initSnapshot();

  if (d->nbThreads == 0)
  {
    scanSingleThreaded();
  }
  else
  {
    scanMultiThreaded();
  }

  if (d->remapFileIds)
  {
    const std::filesystem::path tmp_path = m_snapshot_creator->snapshotWriter()->filePath();

    SnapshotMerger merger;
    merger.setOutputPath(d->outputPath);
    merger.setProjectHome(d->homeDirectory);
    merger.addInputPath(tmp_path);

    m_snapshot_creator.reset();

    merger.runMerge();

    std::filesystem::remove(tmp_path);
  }
}

void Scanner::processCommands(const std::vector<ScannerCompileCommand>& commands, FileIndexingArbiter& arbiter, clang::FileManager& fileManager)
{
  assert(d->fileIdentificator);

  Indexer indexer{ arbiter };
  IndexingFrontendActionFactory actionfactory{ std::make_shared<ForwardingIndexDataConsumer>(&indexer) };
  actionfactory.setIndexLocalSymbols(d->indexLocalSymbols);

  for (const ScannerCompileCommand& cc : commands)
  {
    assert(cc.translated);

    const bool should_parse = passTranslationUnitFilters(cc.fileName);
    const std::optional<std::filesystem::path>& pch_output = cc.pchOutput;
    const std::optional<std::filesystem::path>& pcm_output = cc.pcmOutput;

    if (should_parse)
    {
      std::cout << cc.fileName << std::endl;
    
      if (!std::filesystem::exists(cc.fileName))
      {
        std::cout << "error: file does not exist" << std::endl;
        continue;
      }
    }
    else
    {
      if (pch_output.has_value())
      {
        std::cout << "[PCH] " << cc.fileName << std::endl;

        if (!std::filesystem::exists(cc.fileName))
        {
          std::cout << "error: file does not exist" << std::endl;
          continue;
        }
      }
      else if (pcm_output.has_value())
      {
        std::cout << "[PCM] " << cc.fileName << std::endl;

        if (!std::filesystem::exists(cc.fileName))
        {
          std::cout << "error: file does not exist" << std::endl;
          continue;
        }
      }
      else
      {
        std::cout << "[SKIPPED] " << cc.fileName << std::endl;
      }
    }

    if (pch_output.has_value())
    {
      compilePCH(cc, *pch_output, &fileManager);
    }
    else if (pcm_output.has_value())
    {
      compilePCM(cc, *pcm_output, &fileManager);
    }

    if (!should_parse) {
      continue;
    }

    clang::tooling::ToolInvocation invocation{ cc.commandLine, actionfactory.create(), &fileManager };

    invocation.setDiagnosticConsumer(indexer.getOrCreateDiagnosticConsumer());

    const bool success = invocation.run();

    if (success) {
      // can getCurrentIndex() return nullptr if the "invocation" was a success ?
      if (indexer.getCurrentIndex()) {
        m_snapshot_creator->feed(std::move(*indexer.getCurrentIndex()));
        indexer.resetCurrentIndex();
      }
    } else {
      std::cout << "error: tool invocation failed" << std::endl;
    }
  }
}

void Scanner::fillContent(File& f)
{
  return SnapshotCreator::fillContent(f);
}

} // namespace cppscanner

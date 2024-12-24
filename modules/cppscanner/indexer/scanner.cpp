// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#include "scanner.h"


#include "indexer.h"
#include "indexingresultqueue.h"
#include "workqueue.h"

#include "argumentsadjuster.h"
#include "frontendactionfactory.h"
#include "fileidentificator.h"
#include "fileindexingarbiter.h"
#include "translationunitindex.h"

#include "cppscanner/database/transaction.h"

#include "cppscanner/base/glob.h"
#include "cppscanner/base/os.h"
#include "cppscanner/base/version.h"

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

#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Option/Option.h>
#include <llvm/Support/SHA1.h>
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

struct ScannerData
{
  std::string homeDirectory;
  std::optional<std::string> rootDirectory;
  bool indexExternalFiles = false;
  bool indexLocalSymbols = false;
  size_t nbThreads = 0;
  std::vector<std::string> filters;
  std::vector<std::string> translationUnitFilters;
  std::vector<std::string> compilationArguments;
  bool captureFileContent = true;

  std::vector<ScannerCompileCommand> compileCommands;

  std::unique_ptr<FileIdentificator> fileIdentificator;
  std::vector<bool> filePathsInserted;
  std::vector<bool> indexedFiles;
  std::map<SymbolID, IndexerSymbol> symbols;
};

void set_flag(std::vector<bool>& flags, size_t i)
{
  if (flags.size() <= i) {
    flags.resize(i + 1, false);
  }

  flags[i] = true;
}

bool test_flag(const std::vector<bool>& flags, size_t i)
{
  return i < flags.size() && flags.at(i);
}

std::unique_ptr<FileIndexingArbiter> createIndexingArbiter(ScannerData& d)
{
  std::vector<std::unique_ptr<FileIndexingArbiter>> arbiters;

  arbiters.push_back(std::make_unique<IndexOnceFileIndexingArbiter>(*d.fileIdentificator));

  if (d.indexExternalFiles) {
    if (d.rootDirectory.has_value()) {
      arbiters.push_back(std::make_unique<IndexDirectoryFileIndexingArbiter>(*d.fileIdentificator, *d.rootDirectory));
    }
  } else {
    arbiters.push_back(std::make_unique<IndexDirectoryFileIndexingArbiter>(*d.fileIdentificator, d.homeDirectory));
  }

  if (!d.filters.empty()) {
    arbiters.push_back(std::make_unique<IndexFilesMatchingPatternIndexingArbiter>(*d.fileIdentificator, d.filters));
  }

  return FileIndexingArbiter::createCompositeArbiter(std::move(arbiters));
}

Scanner::Scanner()
{
  d = std::make_unique<ScannerData>();
  d->homeDirectory = std::filesystem::current_path().generic_u8string();
}

Scanner::~Scanner() = default;

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

void Scanner::setCompilationArguments(const std::vector<std::string>& args)
{
  d->compilationArguments = args;
}

/**
 * \brief creates an empty snapshot
 * \param p  the path of the database
 */
void Scanner::initSnapshot(const std::filesystem::path& p)
{
  m_snapshot = std::make_unique<SnapshotWriter>(p);

  m_snapshot->setProperty("cppscanner.version", cppscanner::versioncstr());
  m_snapshot->setProperty("cppscanner.os", cppscanner::system_name());

  m_snapshot->setProperty("project.home", Snapshot::Path(d->homeDirectory));

  m_snapshot->setProperty("scanner.indexExternalFiles", d->indexExternalFiles);
  m_snapshot->setProperty("scanner.indexLocalSymbols", d->indexLocalSymbols);
  m_snapshot->setProperty("scanner.root", Snapshot::Path(d->rootDirectory.value_or(std::string())));
  m_snapshot->setProperty("scanner.workingDirectory", Snapshot::Path(std::filesystem::current_path().generic_u8string()));
}

SnapshotWriter* Scanner::snapshot() const
{
  return m_snapshot.get();
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

static bool isPchCompileCommand(const clang::tooling::CompileCommand& cc)
{
#ifdef _WIN32
  constexpr bool is_windows = true;
#else
  constexpr bool is_windows = false;
#endif // _WIN32

  if constexpr(is_windows)
  {
    auto it = std::find_if(cc.CommandLine.begin(), cc.CommandLine.end(), [](const std::string& arg) {
      return arg.rfind("/Yc", 0) == 0;
      });

    if (it != cc.CommandLine.end()) {
      return true;
    }
  }
  
  return false;
}

static bool isPcmCompileCommand(const clang::tooling::CompileCommand& cc)
{
  if (cc.CommandLine.at(1) == "-cc1")
  {
    return std::find(cc.CommandLine.begin(), cc.CommandLine.end(), "-emit-module-interface") != cc.CommandLine.end();
  }
  else
  {
    return std::find(cc.CommandLine.begin(), cc.CommandLine.end(), "--precompile") != cc.CommandLine.end();
  }
}

void Scanner::scanFromCompileCommands(const std::filesystem::path& compileCommandsPath)
{
  assert(snapshot());

  std::string error_message;
  auto compile_commands = clang::tooling::JSONCompilationDatabase::loadFromFile(
    compileCommandsPath.u8string().c_str(), std::ref(error_message), clang::tooling::JSONCommandLineSyntax::AutoDetect
  );

  if (!compile_commands) {
    std::cerr << "error while parsing compile_commands.json file: " << error_message << std::endl;
    return;
  }

  clang::tooling::ArgumentsAdjuster argsAdjuster = getDefaultArgumentsAdjuster();
  clang::tooling::ArgumentsAdjuster pchArgsAdjuster = getPchArgumentsAdjuster();

  std::cout << "Processing compile_commands.json..." << std::endl;

  for (const clang::tooling::CompileCommand& cc : compile_commands->getAllCompileCommands())
  {
    ScannerCompileCommand scanner_command;
    if (isPchCompileCommand(cc) || isPcmCompileCommand(cc)) {
      scanner_command.commandLine = pchArgsAdjuster(cc.CommandLine, cc.Filename);
    } else {
      scanner_command.commandLine = argsAdjuster(cc.CommandLine, cc.Filename);
    }

    scanner_command.fileName = cc.Filename;

    d->compileCommands.push_back(scanner_command);
  }

  std::cout << "Found " << d->compileCommands.size() << " translation units." << std::endl;

  runScanSingleOrMultiThreaded();
}

void Scanner::scanFromListOfInputs(const std::vector<std::filesystem::path>& inputs)
{
  clang::tooling::ArgumentsAdjuster argsAdjuster = getDefaultArgumentsAdjuster();

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
    scanner_command.commandLine.insert(scanner_command.commandLine.end(), d->compilationArguments.begin(), d->compilationArguments.end());
    scanner_command.commandLine.insert(scanner_command.commandLine.end(), scanner_command.fileName);

    scanner_command.commandLine = argsAdjuster(scanner_command.commandLine, scanner_command.fileName);
   
    d->compileCommands.push_back(std::move(scanner_command));
  }

  if (d->compileCommands.size() > 1)
  {
    std::cout << "Found " << d->compileCommands.size() << " translation units." << std::endl;
  }

  runScanSingleOrMultiThreaded();
}

void Scanner::scan(const std::vector<ScannerCompileCommand>& compileCommands)
{
  d->compileCommands = compileCommands;
  runScanSingleOrMultiThreaded();
}

// converts user-toolchain command line args to clang cc1.
// code largely stolen from clang/Tooling/Tooling.cpp, with 
// some minor adaptations.
class ArgsTranslator
{
private:
  clang::IntrusiveRefCntPtr<clang::FileManager> m_files;
  clang::CompilerInstance m_ci;

public:
  ArgsTranslator()
  {
    m_files = clang::IntrusiveRefCntPtr<clang::FileManager>(new clang::FileManager(clang::FileSystemOptions()));
    m_ci.createDiagnostics();
  }

  explicit ArgsTranslator(clang::FileManager& files) :
    m_files(&files)
  {
    m_ci.createDiagnostics();
  }

  std::vector<std::string> translateArgs(const std::vector<std::string>& args)
  {
    const char* BinaryName = args.front().c_str();

    const auto Driver = newDriver(m_ci.getDiagnostics(), BinaryName, &m_files->getVirtualFileSystem());

    // The "input file not found" diagnostics from the driver are useful.
    // The driver is only aware of the VFS working directory, but some clients
    // change this at the FileManager level instead.
    // In this case the checks have false positives, so skip them.
    if (!m_files->getFileSystemOpts().WorkingDir.empty())
      Driver->setCheckInputsExist(false);

    std::vector<const char*> argv;
    argv.reserve(args.size());
    for (const std::string& a : args)
    {
      argv.push_back(a.c_str());
    }

    const std::unique_ptr<clang::driver::Compilation> Compilation{
      Driver->BuildCompilation(argv)
    };
    
    if (!Compilation)
      return {};

    return getCC1Arguments(*Compilation);
  }

  void translateCommands(std::vector<ScannerCompileCommand>& commands)
  {
    for (ScannerCompileCommand& cc : commands)
    {
      cc.commandLine = translateArgs(cc.commandLine);
    }
  }

  clang::FileManager* fileManager() const
  {
    return m_files.get();
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

static void compilePCH(const ScannerCompileCommand& cc, const std::filesystem::path& pchOutput, clang::FileManager* fileManager)
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

static void compilePCM(const ScannerCompileCommand& cc, const std::filesystem::path& pcmOutput, clang::FileManager* fileManager)
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

static std::optional<std::filesystem::path> findOutput(const ScannerCompileCommand& cc)
{
  auto it = std::find(cc.commandLine.begin(), cc.commandLine.end(), "-o");

  if (it == cc.commandLine.end()) {
    return std::nullopt;
  }

  return std::filesystem::path(*std::next(it));
}

static std::optional<std::filesystem::path> findPchOutput(const ScannerCompileCommand& cc)
{
  auto it = std::find(cc.commandLine.begin(), cc.commandLine.end(), "-emit-pch");

  if (it == cc.commandLine.end()) {
    return std::nullopt;
  }

  return findOutput(cc);
}

static std::optional<std::filesystem::path> findPcmOutput(const ScannerCompileCommand& cc)
{
  auto it = std::find(cc.commandLine.begin(), cc.commandLine.end(), "-emit-module-interface");

  if (it == cc.commandLine.end()) {
    return std::nullopt;
  }

  return findOutput(cc);
}

void Scanner::scanSingleThreaded()
{
  d->fileIdentificator = FileIdentificator::createFileIdentificator();
  std::unique_ptr<FileIndexingArbiter> indexing_arbiter = createIndexingArbiter(*d);
  clang::IntrusiveRefCntPtr<clang::FileManager> file_manager{ new clang::FileManager(clang::FileSystemOptions()) };

  ArgsTranslator translator{ *file_manager };
  translator.translateCommands(d->compileCommands);

  processCommands(d->compileCommands, *indexing_arbiter, *file_manager);
}

static bool run_invocation(const WorkQueue::ToolInvocation& invocation, Indexer& indexer, IndexingFrontendActionFactory& actionfactory, clang::FileManager* fileManager)
{
  if (!std::filesystem::exists(invocation.filename))
  {
    return false;
  }

  clang::tooling::ToolInvocation tool_invocation{ invocation.command, actionfactory.create(), fileManager };
  tool_invocation.setDiagnosticConsumer(indexer.getDiagnosticConsumer());
  return tool_invocation.run();
}

void parsing_thread_proc(ScannerData* data, FileIndexingArbiter* arbiter, WorkQueue* inputQueue, IndexingResultQueue* resultQueue, std::atomic<int>& running)
{
  clang::IntrusiveRefCntPtr<clang::FileManager> file_manager{ new clang::FileManager(clang::FileSystemOptions()) };
  auto index_data_consumer = std::make_shared<Indexer>(*arbiter, *resultQueue);
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
      success = run_invocation(*item, *index_data_consumer, actionfactory, file_manager.get());
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

  d->fileIdentificator = FileIdentificator::createThreadSafeFileIdentificator();

  std::unique_ptr<FileIndexingArbiter> indexing_arbiter = createIndexingArbiter(*d);

  if (d->nbThreads > 1)
  {
    indexing_arbiter = FileIndexingArbiter::createThreadSafeArbiter(std::move(indexing_arbiter));
  }

  ArgsTranslator translator;
  translator.translateCommands(d->compileCommands);

  std::vector<WorkQueue::ToolInvocation> tasks;
  std::vector<ScannerCompileCommand> pch_ccs;

  for (ScannerCompileCommand& cc : d->compileCommands)
  {
    std::optional<std::filesystem::path> pch_output = findPchOutput(cc);

    if (pch_output.has_value())
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
      assimilate(std::move(result));
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
  if (d->nbThreads == 0)
  {
    scanSingleThreaded();
  }
  else
  {
    scanMultiThreaded();
  }
}

void Scanner::processCommands(const std::vector<ScannerCompileCommand>& commands, FileIndexingArbiter& arbiter, clang::FileManager& fileManager)
{
  assert(d->fileIdentificator);

  IndexingResultQueue results_queue;
  auto index_data_consumer = std::make_shared<Indexer>(arbiter, results_queue);
  IndexingFrontendActionFactory actionfactory{ index_data_consumer };
  actionfactory.setIndexLocalSymbols(d->indexLocalSymbols);

  for (const ScannerCompileCommand& cc : commands)
  {
    const bool should_parse = passTranslationUnitFilters(cc.fileName);
    std::optional<std::filesystem::path> pch_output = findPchOutput(cc);
    std::optional<std::filesystem::path> pcm_output = findPcmOutput(cc);

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

    invocation.setDiagnosticConsumer(index_data_consumer->getDiagnosticConsumer());

    const bool success = invocation.run();

    if (success) {
      std::optional<TranslationUnitIndex> result = results_queue.readSync();
      // "result" may be std::nullopt if a fatal error occured while parsing
      // the translation unit.
      if (result.has_value()) {
        assimilate(std::move(result.value()));
      }
    } else {
      std::cout << "error: tool invocation failed" << std::endl;
    }
  }
}

namespace
{

std::set<FileID> listIncludedFiles(const std::vector<Include>& includes)
{
  std::set<FileID> ids;
  
  for (const Include& incl : includes) {
    ids.insert(incl.includedFileID);
  }

  return ids;
}

std::vector<Include> merge(
  std::vector<Include>&& existingIncludes, 
  std::vector<Include>::const_iterator newIncludesBegin, 
  std::vector<Include>::const_iterator newIncludesEnd)
{
  existingIncludes.insert(existingIncludes.end(), newIncludesBegin, newIncludesEnd);

  auto comp = [](const Include& a, const Include& b) -> bool {
    return std::forward_as_tuple(a.fileID, a.line) < std::forward_as_tuple(b.fileID, b.line);
    };

  std::sort(existingIncludes.begin(),  existingIncludes.end(), comp);

  auto eq = [](const Include& a, const Include& b) -> bool {
    return std::forward_as_tuple(a.fileID, a.line) == std::forward_as_tuple(b.fileID, b.line);
    };

  auto new_logical_end = std::unique(existingIncludes.begin(), existingIncludes.end(), eq);
  existingIncludes.erase(new_logical_end, existingIncludes.end());

  return existingIncludes;
}

std::vector<SymbolReference>& insertOrIgnore(
  std::vector<SymbolReference>& references, 
  std::vector<SymbolReference>::const_iterator newReferencesBegin, 
  std::vector<SymbolReference>::const_iterator newReferencesEnd)
{
  references.insert(references.end(), newReferencesBegin, newReferencesEnd);

  auto ref_comp = [](const SymbolReference& a, const SymbolReference& b) -> bool {
    return std::forward_as_tuple(a.fileID, a.position, a.symbolID) < std::forward_as_tuple(b.fileID, b.position, b.symbolID);
    };

  std::sort(references.begin(),  references.end(), ref_comp);

  auto ref_eq = [](const SymbolReference& a, const SymbolReference& b) -> bool {
    return std::forward_as_tuple(a.fileID, a.position, a.symbolID) == std::forward_as_tuple(b.fileID, b.position, b.symbolID);
    };

  auto new_logical_end = std::unique(references.begin(), references.end(), ref_eq);
  references.erase(new_logical_end, references.end());

  return references;
}

inline FileID fileOf(const Diagnostic& d)
{
  return d.fileID;
}

inline FileID fileOf(const SymbolDeclaration& d)
{
  return d.fileID;
}

template<typename T>
void sortByFile(std::vector<T>& elements)
{
  std::sort(
    elements.begin(), 
    elements.end(), 
    [](const T& a, const T& b) -> bool {
      return fileOf(a) < fileOf(b);
    }
  );
}

template<typename T>
typename std::vector<T>::iterator fileRangeEnd(std::vector<T>& elements, typename std::vector<T>::iterator begin)
{
  FileID cur_file_id = fileOf(*begin);
  return std::find_if(std::next(begin), elements.end(),
    [cur_file_id](const T& element) {
      return fileOf(element) != cur_file_id;
    }
  );
}

bool mergeComp(const Diagnostic& a, const Diagnostic& b)
{
  return std::forward_as_tuple(a.level, a.position, a.message) <
    std::forward_as_tuple(b.level, b.position, b.message);
}

bool mergeComp(const SymbolDeclaration& a, const SymbolDeclaration& b)
{
  // we ignore file id because it is assumed they are the same
  assert(a.fileID == b.fileID);
  return std::forward_as_tuple(a.symbolID, a.startPosition, a.endPosition, a.isDefinition) <
    std::forward_as_tuple(b.symbolID, b.startPosition, b.endPosition, b.isDefinition);
}

template<typename T>
bool mergeEq(const T& a, const T& b)
{
  return !mergeComp(a, b) && !mergeComp(b, a);
}

template<typename T>
std::vector<T> merge(
  std::vector<T>&& existingElements, 
  typename std::vector<T>::const_iterator newElementsBegin, 
  typename std::vector<T>::const_iterator newElementsEnd)
{
  existingElements.insert(existingElements.end(), newElementsBegin, newElementsEnd);
  std::sort(existingElements.begin(), existingElements.end(), [](const T& a, const T& b) { return mergeComp(a, b); });
  auto new_logical_end = std::unique(existingElements.begin(), existingElements.end(), [](const T& a, const T& b) { return mergeEq(a, b); });
  existingElements.erase(new_logical_end, existingElements.end());
  return existingElements;
}

void removeCarriageReturns(std::string& text)
{
#ifndef _WIN32
  return;
#endif // !_WIN32

  auto it = std::remove_if(text.begin(), text.end(), [](char c) {
    return c == '\r';
    });

  text.erase(it, text.end());
}

std::string computeSha1(const std::string& text)
{
  llvm::SHA1 hasher;
  hasher.update(text);
  std::array<uint8_t, 20> result = hasher.final();
  constexpr bool to_lower_case = true;
  return llvm::toHex(result, to_lower_case);
}

} // namespace

void Scanner::fillContent(File& f)
{
  {
    std::ifstream stream{ f.path };
    if (stream.good()) 
    {
      stream.seekg(0, std::ios::end);
      f.content.reserve(stream.tellg());
      stream.seekg(0, std::ios::beg);
      f.content.assign((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    }
  }

  if (!f.content.empty())
  {
    removeCarriageReturns(f.content);
    f.sha1 = computeSha1(f.content);
  }
}

void Scanner::assimilate(TranslationUnitIndex tuIndex)
{
  const std::vector<std::string> known_files = d->fileIdentificator->getFiles();

  std::vector<File> newfiles;

  {
    for (FileID fid : tuIndex.indexedFiles)
    {
      if (fileAlreadyIndexed(fid)) {
        continue;
      }

      File f;
      f.id = fid;
      f.path = known_files.at(fid);

      if (d->captureFileContent)
      {
        fillContent(f);
      }

      newfiles.push_back(std::move(f));
    }

    {
      sql::TransactionScope transaction{ m_snapshot->database() };
      m_snapshot->insertFiles(newfiles);
    }

    for (const File& f : newfiles) {
      set_flag(d->filePathsInserted, f.id);
    }
  }

  {
    // ensure that included files are listed in the database
    {
      std::set<FileID> included = listIncludedFiles(tuIndex.ppIncludes);
      std::vector<File> newincludes;

      for (FileID fid : included)
      {
        if (test_flag(d->filePathsInserted, fid)) {
          continue;
        }

        File f;
        f.id = fid;
        f.path = known_files.at(fid);
        newincludes.push_back(std::move(f));
      }

      sql::runTransacted(m_snapshot->database(), [this, &newincludes]() {
        m_snapshot->insertFilePaths(newincludes);
        });

      for (const File& f : newincludes) {
        set_flag(d->filePathsInserted, f.id);
      }
    }
    
    std::sort(tuIndex.ppIncludes.begin(), tuIndex.ppIncludes.end(), [](const Include& a, const Include& b) {
      return a.fileID < b.fileID;
      });

    auto it = tuIndex.ppIncludes.begin();

    while (it != tuIndex.ppIncludes.end()) {
      FileID cur_file_id = it->fileID;
      auto end = std::find_if(std::next(it), tuIndex.ppIncludes.end(),
        [cur_file_id](const Include& element) {
          return element.fileID != cur_file_id;
        }
      );

      if (fileAlreadyIndexed(cur_file_id)) {
        std::vector<Include> includes = merge(m_snapshot->loadAllIncludesInFile(cur_file_id), it, end);

        sql::runTransacted(m_snapshot->database(), [this, cur_file_id, &includes]() {
          m_snapshot->removeAllIncludesInFile(cur_file_id);
          m_snapshot->insertIncludes(includes);
          });
      } else {
        sql::runTransacted(m_snapshot->database(), [this, it, end]() {
          m_snapshot->insertIncludes(std::vector<Include>(it, end));
          });
      }

      it = end;
    }
  }

  // insert new symbols, update the others that need it
  {
    std::vector<const IndexerSymbol*> symbols_to_insert;
    std::vector<const IndexerSymbol*> symbols_with_flags_to_update;

    for (auto& p : tuIndex.symbols) {
      auto it = d->symbols.find(p.first);
      if (it == d->symbols.end()) {
        IndexerSymbol& newsymbol = d->symbols[p.first];
        newsymbol = std::move(p.second);
        symbols_to_insert.push_back(&newsymbol);
      } else {
        IndexerSymbol& existing_symbol = it->second;
        int what_updated = update(existing_symbol, p.second);
        if (what_updated & IndexerSymbol::FlagUpdate)
        {
          symbols_with_flags_to_update.push_back(&existing_symbol);
        }
      }
    }

    sql::runTransacted(m_snapshot->database(), [this, &symbols_to_insert, &symbols_with_flags_to_update]() {
      m_snapshot->insertSymbols(symbols_to_insert);
      m_snapshot->updateSymbolsFlags(symbols_with_flags_to_update);
      });
  }

  // Process symbol references
  {
    auto it = tuIndex.symReferences.begin();

    while (it != tuIndex.symReferences.end()) {
      FileID cur_file_id = it->fileID;

      auto end = std::find_if(std::next(it), tuIndex.symReferences.end(),
        [cur_file_id](const SymbolReference& ref) {
          return ref.fileID != cur_file_id;
        }
      );

      if (fileAlreadyIndexed(cur_file_id)) 
      {
        std::vector<SymbolReference> references = m_snapshot->loadSymbolReferencesInFile(cur_file_id);
        insertOrIgnore(references, it, end);

        {
          sql::TransactionScope transaction{ m_snapshot->database() };
          m_snapshot->removeAllSymbolReferencesInFile(cur_file_id);
          m_snapshot->insert(references);
        }
      } 
      else 
      {
        sql::TransactionScope transaction{ m_snapshot->database() };
        m_snapshot->insert(std::vector<SymbolReference>(it, end));
      }

      it = end;
    }
  }

  // Process relations
  {
    m_snapshot->insertBaseOfs(tuIndex.relations.baseOfs);
    m_snapshot->insertOverrides(tuIndex.relations.overrides);
  }

  // Process diagnostics
  {
    sortByFile(tuIndex.diagnostics);

    for (auto it = tuIndex.diagnostics.begin(); it != tuIndex.diagnostics.end(); ) {
      FileID cur_file_id = it->fileID;
      auto end = fileRangeEnd(tuIndex.diagnostics, it);

      if (fileAlreadyIndexed(cur_file_id)) {
        std::vector<Diagnostic> diagnostics = merge(m_snapshot->loadDiagnosticsInFile(cur_file_id), it, end);

        sql::runTransacted(m_snapshot->database(), [this, cur_file_id, &diagnostics]() {
          m_snapshot->removeAllDiagnosticsInFile(cur_file_id);
          m_snapshot->insertDiagnostics(diagnostics);
          });
      } else {
        sql::runTransacted(m_snapshot->database(), [this, it, end]() {
          m_snapshot->insertDiagnostics(std::vector<Diagnostic>(it, end));
          });
      }

      it = end;
    }
  }

  // Process refargs
  {
    sql::TransactionScope transaction{ m_snapshot->database() };
    m_snapshot->insert(tuIndex.fileAnnotations.refargs);
  }

  // Process symbol declarations
  {
    // declarations are sorted by file.
#ifndef  NDEBUG
    bool sorted = std::is_sorted(tuIndex.declarations.begin(), tuIndex.declarations.end(), 
      [](const SymbolDeclaration& a, const SymbolDeclaration& b) {
        return a.fileID < b.fileID; 
      });
    assert(sorted);
#endif // !NDEBUG

    for (auto it = tuIndex.declarations.begin(); it != tuIndex.declarations.end(); ) {
      FileID cur_file_id = it->fileID;
      auto end = fileRangeEnd(tuIndex.declarations, it);

      if (fileAlreadyIndexed(cur_file_id)) {
        std::vector<SymbolDeclaration> declarations = merge(m_snapshot->loadDeclarationsInFile(cur_file_id), it, end);

        sql::TransactionScope transaction{ m_snapshot->database() };
        m_snapshot->removeAllDeclarationsInFile(cur_file_id);
        m_snapshot->insert(declarations);
      } else {
        sql::TransactionScope transaction{ m_snapshot->database() };
        m_snapshot->insert(std::vector<SymbolDeclaration>(it, end));
      }

      it = end;
    }    
  }

  // Update list of already indexed files
  for (const File& f : newfiles) {
    setFileIndexed(f.id);
  }
}

bool Scanner::fileAlreadyIndexed(FileID f) const
{
  return d->indexedFiles.size() > f && d->indexedFiles.at(f);
}

void Scanner::setFileIndexed(FileID f)
{
  if (d->indexedFiles.size() <= f) {
    d->indexedFiles.resize(f + 1, false);
  }

  d->indexedFiles[f] = true;
}

} // namespace cppscanner

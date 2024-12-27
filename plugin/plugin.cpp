
#include "cppscanner/indexer/fileidentificator.h"
#include "cppscanner/indexer/fileindexingarbiter.h"
#include "cppscanner/indexer/indexer.h"
#include "cppscanner/indexer/snapshotcreator.h"

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendPluginRegistry.h>
#include <clang/Frontend/FrontendActions.h>

#include <clang/Index/IndexingAction.h>

#include <cstdlib> // getenv()
#include <cstring> // strcmp(), strlen()
#include <filesystem>
#include <iostream>

namespace cppscanner
{

// an index data consumer that bundles an Indexer.
// this allows the Indexer to live as long as the ForwardingIndexDataConsumer
// lives (and is useful).
// indeed, the clang::PluginASTAction creates the ASTConsumer (that uses the
// IndexDataConsumerBundle), but it is destroyed before the ASTConsumer.
class IndexDataConsumerBundle : public ForwardingIndexDataConsumer
{
private:
  std::string m_homePath;
  std::filesystem::path m_outDir;
  std::string m_inFile;
  bool m_indexLocalSymbols;
  std::unique_ptr<cppscanner::FileIdentificator> m_identificator;
  std::unique_ptr<cppscanner::FileIndexingArbiter> m_arbiter;
  cppscanner::Indexer m_indexer;
  SnapshotCreator m_snapshot_creator;

public:
  IndexDataConsumerBundle(const std::filesystem::path& outDir, std::string inFile, std::string homePath, bool indexLocalSymbols)
    : m_homePath(std::move(homePath))
    , m_outDir(outDir)
    , m_inFile(std::move(inFile))
    , m_indexLocalSymbols(indexLocalSymbols)
    , m_identificator(cppscanner::FileIdentificator::createFileIdentificator())
    , m_arbiter(createArbiter(m_homePath, *m_identificator))
    , m_indexer(*m_arbiter)
    , m_snapshot_creator(*m_identificator)
  {
    setIndexer(m_indexer);
  }

  void initialize(clang::ASTContext& Ctx) final
  {
    std::cout << "initialize()" << std::endl;

    ForwardingIndexDataConsumer::initialize(Ctx);

    m_snapshot_creator.setHomeDir(m_homePath);
    m_snapshot_creator.setCaptureFileContent(false);
 
    if (!std::filesystem::exists(m_outDir))
    {
      std::filesystem::create_directories(m_outDir);
    }

    const std::string filename = std::filesystem::path(m_inFile).filename().string();
    int n = 1;
    std::filesystem::path db_path = m_outDir / (filename + ".1.aba");
    while (std::filesystem::exists(db_path))
    {
      db_path = m_outDir / (filename + "." + std::to_string(++n) + ".aba");
    }

    m_snapshot_creator.init(db_path);

    m_snapshot_creator.writeProperty("scanner.indexLocalSymbols", m_indexLocalSymbols ? "true" : "false");
    m_snapshot_creator.writeProperty("plugin.inFile", m_inFile);
  }

  void finish() final
  {
    std::cout << "finish()" << std::endl;

    ForwardingIndexDataConsumer::finish();

    m_snapshot_creator.feed(std::move(*m_indexer.getCurrentIndex()));

    m_snapshot_creator.close();
  }

private:
  static std::unique_ptr<FileIndexingArbiter> createArbiter(const std::string& homePath, cppscanner::FileIdentificator& fileIdentificator)
  {
    cppscanner::CreateIndexingArbiterOptions opts;
    opts.homeDirectory = homePath;
    opts.indexExternalFiles = false;
    return cppscanner::createIndexingArbiter(fileIdentificator, opts);
  }
};

// a diagnostic consumer that forwards diagnostic to:
// a) an existing diagnostic consumer, and 
// b) the Indexer in a IndexDataConsumerBundle, if it's still alive.
// indeed, because ownership is transferred to a clang::DiagnosticEngine,
// an instance of this class may outlive the Indexer for which it was created.
class ForwardingDiagnosticConsumer : public clang::DiagnosticConsumer
{
private:
  std::weak_ptr<IndexDataConsumerBundle> m_dataConsumer;
  std::unique_ptr<clang::DiagnosticConsumer> m_target;

public:
  ForwardingDiagnosticConsumer(std::shared_ptr<IndexDataConsumerBundle> dataConsumer, std::unique_ptr<clang::DiagnosticConsumer> target)
    : m_dataConsumer(dataConsumer), m_target(std::move(target))
  {

  }

  void HandleDiagnostic(clang::DiagnosticsEngine::Level DiagLevel,
    const clang::Diagnostic &Info) final
  {
    m_target->HandleDiagnostic(DiagLevel, Info);

    auto data_consumer = m_dataConsumer.lock();

    if (data_consumer)
    {
      Indexer& indexer = data_consumer->indexer();

      if (indexer.initialized())
      {
        indexer.HandleDiagnostic(DiagLevel, Info);
      }
    }
  }

  void clear() final
  {
    return m_target->clear();
  }

  bool IncludeInDiagnosticCounts() const final
  {
    return m_target->IncludeInDiagnosticCounts();
  }

  void BeginSourceFile(const clang::LangOptions& LangOpts,
    const clang::Preprocessor* PP) final
  {
    m_target->BeginSourceFile(LangOpts, PP);
  }

  void EndSourceFile() final
  {
    m_target->EndSourceFile();
  }

  void finish() final
  {
    m_target->finish();
  }
};

} // namespace cppscanner

// the plugin action.
// note, the action shouldn't hold state (beyond what is used to create the ASTConsumer) 
// as it is destroyed just after the ASTConsumer is created and before the ast is fully consumed.
class TakeSnapshotPluginASTAction : public clang::PluginASTAction 
{
public:
  TakeSnapshotPluginASTAction();
  ~TakeSnapshotPluginASTAction();

  clang::PluginASTAction::ActionType getActionType() final;

  bool ParseArgs(const clang::CompilerInstance& ci, const std::vector<std::string>& args) final;

  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& ci, llvm::StringRef inFile) final;

  bool BeginSourceFileAction(clang::CompilerInstance &CI) final;
  void EndSourceFileAction() final;

private:
  bool m_indexLocalSymbols = false;
  std::string m_homePath;
  std::string m_outDir;
};

TakeSnapshotPluginASTAction::TakeSnapshotPluginASTAction()
{
  std::cout << "TakeSnapshotPluginASTAction()" << std::endl;

}

TakeSnapshotPluginASTAction::~TakeSnapshotPluginASTAction()
{
  std::cout << "~TakeSnapshotPluginASTAction()" << std::endl;
}

// As far as I can tell, the action object is destroyed after this function is called,
// so the returned ASTConsumer outlives the action.
// The PluginASTAction therefore cannot hold state and the ASTConsumer must be able
// to live on its own.
std::unique_ptr<clang::ASTConsumer> TakeSnapshotPluginASTAction::CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef inFile) 
{
  std::cout << "CreateASTConsumer()" << std::endl;

  std::cout << "inFile = " << inFile.str() << std::endl;

  auto data_consumer = std::make_shared<cppscanner::IndexDataConsumerBundle>(m_outDir, inFile.str(), m_homePath, m_indexLocalSymbols);

  if (ci.getDiagnostics().getClient())
  {
    if (ci.getDiagnostics().ownsClient())
    {
      auto client = ci.getDiagnostics().takeClient();
      auto forwarding_client = std::make_unique<cppscanner::ForwardingDiagnosticConsumer>(data_consumer, std::move(client));
      constexpr bool should_own_client = true;
      ci.getDiagnostics().setClient(forwarding_client.release(), should_own_client);
    }
    else
    {
      // we can't safely forward the diagnostics to the current client as we do not
      // known when it could be destroyed.
      // the only safe thing to do is to do nothing. the diagnostics won't be part of
      // the snapshot.
    }
  }
  else
  {
    constexpr bool should_own_client = false;
    ci.getDiagnostics().setClient(data_consumer->indexer().getOrCreateDiagnosticConsumer(), should_own_client);
  }

  clang::index::IndexingOptions opts; // TODO: review which options we should enable
  opts.IndexFunctionLocals = m_indexLocalSymbols;
  opts.IndexParametersInDeclarations = false;
  opts.IndexTemplateParameters = m_indexLocalSymbols;

  opts.ShouldTraverseDecl = [data_consumer](const clang::Decl* decl) -> bool {
    return data_consumer->indexer().ShouldTraverseDecl(decl);
    };

  return clang::index::createIndexingASTConsumer(data_consumer, opts, ci.getPreprocessorPtr());
}

bool TakeSnapshotPluginASTAction::ParseArgs(const clang::CompilerInstance &ci, const std::vector<std::string>& args)
{
  std::cout << "ParseArgs()" << std::endl;

  bool indexLocalSymbols = false;
  std::optional<std::string> homePath;
  std::optional<std::string> outdir;

  for (const std::string& a : args)
  {
    if (a == "index-local-symbols")
    {
      indexLocalSymbols = true;
    }
    else if (a.rfind("home=", 0) == 0)
    {
      homePath = a.substr(std::strlen("home="));
    }
    else if (a.rfind("outputDir=", 0) == 0)
    {
      outdir = a.substr(std::strlen("outputDir="));
    }
  }

  if (indexLocalSymbols)
  {
    m_indexLocalSymbols = true;
  }
  else
  {
    if (const char* val = std::getenv("CPPSCANNER_INDEX_LOCAL_SYMBOLS"))
    {
      m_indexLocalSymbols = std::strcmp(val, "0") != 0;
    }
  }

  if (homePath.has_value())
  {
    m_homePath = *homePath;
  }
  else
  {
    if (const char* val = std::getenv("CPPSCANNER_HOME_DIR"))
    {
      m_homePath = val;
    }
    else
    {
      m_homePath = std::filesystem::current_path().generic_u8string();
    }
  }

  if (outdir.has_value())
  {
    m_outDir = *outdir;
  }
  else
  {
    if (const char* val = std::getenv("CPPSCANNER_OUTPUT_DIR"))
    {
      m_outDir = val;
    }
    else
    {
      m_outDir = (std::filesystem::current_path() / ".cppscanner").generic_u8string();
    }
  }
  
  std::cout << "m_homePath = " << m_homePath << std::endl;
  std::cout << "m_outDir = " << m_outDir << std::endl;

  return true;
}

clang::PluginASTAction::ActionType TakeSnapshotPluginASTAction::getActionType()
{
  return AddBeforeMainAction;
}

// As far as I understand, BeginSourceFileAction() and EndSourceFileAction() are only called
// if getActionType() is clang::PluginASTAction::ReplaceAction, so we shouldn't do anything in these.
bool TakeSnapshotPluginASTAction::BeginSourceFileAction(clang::CompilerInstance &CI)
{
  return clang::PluginASTAction::BeginSourceFileAction(CI);
}

void TakeSnapshotPluginASTAction::EndSourceFileAction()
{
  clang::PluginASTAction::EndSourceFileAction();
}

clang::FrontendPluginRegistry::Add<TakeSnapshotPluginASTAction> X("cppscanner", "Save a snapshot of a translation unit.");



#include "cppscanner/indexer/fileidentificator.h"
#include "cppscanner/indexer/fileindexingarbiter.h"
#include "cppscanner/indexer/indexer.h"
#include "cppscanner/indexer/snapshotcreator.h"

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendPluginRegistry.h>
#include <clang/Frontend/FrontendActions.h>

#include <clang/Index/IndexingAction.h>

#include <cstdlib> // getenv()
#include <cstring> // strcmp()
#include <filesystem>
#include <iostream>

namespace cppscanner
{

class IndexDataConsumerBundle : public ForwardingIndexDataConsumer
{
private:
  std::string m_homePath;
  std::string m_dbPath;
  bool m_indexLocalSymbols;
  std::unique_ptr<cppscanner::FileIdentificator> m_identificator;
  std::unique_ptr<cppscanner::FileIndexingArbiter> m_arbiter;
  cppscanner::Indexer m_indexer;
  SnapshotCreator m_snapshot_creator;

public:
  IndexDataConsumerBundle(std::string dbPath, std::string homePath, bool indexLocalSymbols)
    : m_homePath(std::move(homePath))
    , m_dbPath(std::move(dbPath))
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
    if (std::filesystem::exists(m_dbPath))
    {
      std::filesystem::remove(m_dbPath);
    }
    m_snapshot_creator.init(m_dbPath);

    m_snapshot_creator.writeProperty("scanner.indexLocalSymbols", m_indexLocalSymbols ? "true" : "false");
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

} // namespace cppscanner

// note, the action shouldn't hold state as it is destroyed after the ast consumer is created
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
std::unique_ptr<clang::ASTConsumer> TakeSnapshotPluginASTAction::CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef inFile) 
{
  std::cout << "CreateASTConsumer()" << std::endl;

  std::cout << "inFile = " << inFile.str() << std::endl;

  auto data_consumer = std::make_shared<cppscanner::IndexDataConsumerBundle>(inFile.str() + ".cppscanner.db", m_homePath, m_indexLocalSymbols);

  // TODO: see how we can handle diagnostics
  //constexpr bool should_own_client = false;
  //ci.getDiagnostics().setClient(idc->getDiagnosticConsumer(), should_own_client);
  if (ci.getDiagnostics().getClient())
  {
    std::cout << "diagnostic engine has a client\n";
  }
  else
  {
    std::cout << "diagnostic engine doesn't have a client\n";
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

  if (const char* val = std::getenv("CPPSCANNER_INDEX_LOCAL_SYMBOLS"))
  {
    m_indexLocalSymbols = std::strcmp(val, "0") != 0;
  }

  if (const char* val = std::getenv("CPPSCANNER_HOME_DIR"))
  {
    m_homePath = val;
  }
  else
  {
    m_homePath = std::filesystem::current_path().generic_u8string();
  }

  std::cout << "m_homePath = " << m_homePath << std::endl;

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
  std::cout << "BeginSourceFileAction()" << std::endl;

  return clang::PluginASTAction::BeginSourceFileAction(CI);
}

void TakeSnapshotPluginASTAction::EndSourceFileAction()
{
  std::cout << "EndSourceFileAction()" << std::endl;

  clang::PluginASTAction::EndSourceFileAction();
}

clang::FrontendPluginRegistry::Add<TakeSnapshotPluginASTAction> X("cppscanner", "Save a snapshot of a translation unit.");


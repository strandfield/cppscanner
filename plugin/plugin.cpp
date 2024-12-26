
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

class PluginIndexer : public Indexer
{
private:
  std::string m_homePath;
  std::string m_dbPath;
  SnapshotCreator m_snapshot_creator;

public:
  PluginIndexer(std::string dbPath, std::string homePath, FileIndexingArbiter& arbiter) : Indexer(arbiter),
    m_snapshot_creator(arbiter.fileIdentificator()),
    m_homePath(homePath),
    m_dbPath(dbPath)
  {
    std::cout << "PluginIndexer()" << std::endl;

  }

  ~PluginIndexer()
  {
    std::cout << "~PluginIndexer()" << std::endl;
  }

  void initialize(clang::ASTContext& Ctx) final
  {
    std::cout << "initialize()" << std::endl;


    Indexer::initialize(Ctx);

    m_snapshot_creator.setHomeDir(m_homePath);
    m_snapshot_creator.setCaptureFileContent(false);
    m_snapshot_creator.init(m_dbPath);
  }

  void finish()  final
  {
    std::cout << "finish()" << std::endl;


    Indexer::finish();

    m_snapshot_creator.feed(std::move(*getCurrentIndex()));

    m_snapshot_creator.close();
  }
};

} // namespace cppscanner

class TakeSnapshotPluginASTAction : public clang::PluginASTAction 
{
public:
  TakeSnapshotPluginASTAction();
  ~TakeSnapshotPluginASTAction();

  clang::PluginASTAction::ActionType getActionType() final;

  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& ci, llvm::StringRef inFile) final;
  bool ParseArgs(const clang::CompilerInstance& ci, const std::vector<std::string>& args) final;

  bool BeginSourceFileAction(clang::CompilerInstance &CI) final;
  void EndSourceFileAction() final;
  
private:
  bool m_indexLocalSymbols = false;
  std::string m_homePath;
  std::unique_ptr<cppscanner::FileIdentificator> m_identificator;
  std::unique_ptr<cppscanner::FileIndexingArbiter> m_arbiter;
};

TakeSnapshotPluginASTAction::TakeSnapshotPluginASTAction()
{

}

TakeSnapshotPluginASTAction::~TakeSnapshotPluginASTAction()
{
  std::cout << "~TakeSnapshotPluginASTAction()" << std::endl;
}

std::unique_ptr<clang::ASTConsumer> TakeSnapshotPluginASTAction::CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef inFile) 
{
  std::cout << "CreateASTConsumer()" << std::endl;

  std::cout << "inFile = " << inFile.str() << std::endl;

  auto indexer = std::make_shared<cppscanner::PluginIndexer>(inFile.str() + ".cppscanner.db", m_homePath, *m_arbiter);

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

  opts.ShouldTraverseDecl = [indexer](const clang::Decl* decl) -> bool {
    return indexer->ShouldTraverseDecl(decl);
    };

  return clang::index::createIndexingASTConsumer(indexer, opts, ci.getPreprocessorPtr());
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

  m_identificator = cppscanner::FileIdentificator::createFileIdentificator();
  cppscanner::CreateIndexingArbiterOptions opts;
  opts.homeDirectory = m_homePath;
  opts.indexExternalFiles = false;
  m_arbiter = cppscanner::createIndexingArbiter(*m_identificator, opts);
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


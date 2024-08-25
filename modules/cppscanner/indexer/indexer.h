// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_INDEXER_H
#define CPPSCANNER_INDEXER_H

#include "translationunitindex.h"

#include "cppscanner/index/fileid.h"

#include <clang/Basic/Diagnostic.h>
#include <clang/Index/IndexDataConsumer.h>

#include <map>
#include <memory>

namespace cppscanner
{

class FileIndexingArbiter;
class FileIdentificator;
class IndexingResultQueue;

class Indexer;

/**
 * \brief a class for collecting C++ symbols while indexing a translation unit
 */
class SymbolCollector
{
private:
  Indexer& m_indexer;
  std::map<const void*, SymbolID> m_symbolIdCache;

public:
  explicit SymbolCollector(Indexer& idxr);

  void reset();

  Symbol* process(const clang::Decl* decl);
  Symbol* process(const clang::IdentifierInfo* name, const clang::MacroInfo* macroInfo, clang::SourceLocation loc);

  SymbolID getSymbolId(const clang::Decl* decl) const;

protected:
  std::string getDeclSpelling(const clang::Decl* decl);
  void fillSymbol(Symbol& symbol, const clang::Decl* decl);
  void fillSymbol(Symbol& symbol,const clang::IdentifierInfo* name, const clang::MacroInfo* macroInfo);
  void fillDisplayName(Symbol& symbol, const clang::Decl* decl);
  Symbol* getParentSymbol(const Symbol& symbol, const clang::Decl* decl);
};

/**
 * \brief class for receiving diagnostics from clang
 */
class IdxrDiagnosticConsumer : public clang::DiagnosticConsumer
{
  Indexer& m_indexer;

public:
  explicit IdxrDiagnosticConsumer(Indexer& idxr) : 
    m_indexer(idxr)
  {
  }

  bool IncludeInDiagnosticCounts() const final { return false; }

  void HandleDiagnostic(clang::DiagnosticsEngine::Level dlvl, const clang::Diagnostic& dinfo) final;
  void finish() final;
};

/**
 * \brief class for receiving indexing data from clang
 */
class Indexer : public clang::index::IndexDataConsumer
{
private:
  FileIndexingArbiter& m_fileIndexingArbiter;
  IndexingResultQueue& m_resultsQueue;
  FileIdentificator& m_fileIdentificator;
  SymbolCollector m_symbolCollector;
  IdxrDiagnosticConsumer m_diagnosticConsumer;
  clang::ASTContext* mAstContext = nullptr;
  std::shared_ptr<clang::Preprocessor> m_pp;
  std::unique_ptr<TranslationUnitIndex> m_index;
  std::map<clang::FileID, bool> m_ShouldIndexFileCache;
  std::map<clang::FileID, cppscanner::FileID> m_FileIdCache;

public:

  Indexer(FileIndexingArbiter& fileIndexingArbiter, IndexingResultQueue& resultsQueue);
  ~Indexer();

  FileIdentificator& fileIdentificator();

  clang::DiagnosticConsumer* getDiagnosticConsumer();

  TranslationUnitIndex* getCurrentIndex() const;

  bool shouldIndexFile(clang::FileID fileId);
  bool ShouldTraverseDecl(const clang::Decl* decl);
  cppscanner::FileID getFileID(clang::FileID clangFileId);
  clang::ASTContext* getAstContext() const;
  bool initialized() const;

  void initialize(clang::ASTContext& Ctx) final;
  void setPreprocessor(std::shared_ptr<clang::Preprocessor> PP) final;
  bool handleDeclOccurrence(const clang::Decl* D, clang::index::SymbolRoleSet Roles,
    llvm::ArrayRef<clang::index::SymbolRelation> relations,
    clang::SourceLocation Loc, ASTNodeInfo ASTNode) final;
  bool handleMacroOccurrence(const clang::IdentifierInfo* Name,
    const clang::MacroInfo* MI, clang::index::SymbolRoleSet Roles,
    clang::SourceLocation Loc)  final;
  bool handleModuleOccurrence(const clang::ImportDecl* ImportD,
    const clang::Module* Mod, clang::index::SymbolRoleSet Roles,
    clang::SourceLocation Loc)  final;
  void finish() final;

protected:
  friend IdxrDiagnosticConsumer;
  clang::SourceManager& getSourceManager() const;

protected:
  void processRelations(std::pair<const clang::Decl*, Symbol*> declAndSymbol, clang::SourceLocation refLocation, llvm::ArrayRef<clang::index::SymbolRelation> relations);
  void indexPreprocessingRecord(clang::Preprocessor& pp);
};

} // namespace cppscanner

#endif // CPPSCANNER_INDEXER_H

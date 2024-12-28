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
#include <utility>

namespace cppscanner
{

class FileIndexingArbiter;
class FileIdentificator;

class Indexer;

/**
 * \brief an index data consumer that forwards data to an indexer 
 */
class ForwardingIndexDataConsumer : public clang::index::IndexDataConsumer
{
public:
  explicit ForwardingIndexDataConsumer(Indexer* indexer = nullptr);
  
  Indexer& indexer() const;
  void setIndexer(Indexer& indexer);

protected: // clang::index::IndexDataConsumer overrides
  void initialize(clang::ASTContext& Ctx) override;
  void setPreprocessor(std::shared_ptr<clang::Preprocessor> PP) override;
  bool handleDeclOccurrence(const clang::Decl* D, clang::index::SymbolRoleSet Roles,
    llvm::ArrayRef<clang::index::SymbolRelation> relations,
    clang::SourceLocation Loc, ASTNodeInfo ASTNode) override;
  bool handleMacroOccurrence(const clang::IdentifierInfo* Name,
    const clang::MacroInfo* MI, clang::index::SymbolRoleSet Roles,
    clang::SourceLocation Loc)  override;
  bool handleModuleOccurrence(const clang::ImportDecl* ImportD,
    const clang::Module* Mod, clang::index::SymbolRoleSet Roles,
    clang::SourceLocation Loc)  override;
  void finish() override;

private:
  Indexer* m_indexer = nullptr;
};

class IdxrDiagnosticConsumer;
class SymbolCollector;

/**
 * \brief class for indexing translation units
 * 
 * This class is used to index a translation unit by filling a 
 * corresponding TranslationUnitIndex.
 * 
 * The main role of this class is to collect references to symbols 
 * and macros in handleDeclOccurrence() and handleMacroOccurrence().
 * When a symbol or macro is first encountered, SymbolCollector is 
 * used to create a IndexerSymbol object representing the symbol.
 * 
 * When the indexing of the translation unit is about to be completed
 * in finish(), the Indexer does some post-processing:
 * - list the files that were included by the clang::Preprocessor,
 * - check whether macros were used as header guards,
 * - visit the ast to list places where arguments are passed by reference,
 * - and collect the precise location of some symbol's declarations.
 *
 * Compiler diagnostics can be passed to the HandleDiagnostic() function.
 * The getOrCreateDiagnosticConsumer() function returns a diagnostic consumer
 * that forwards diagnostics to the Indexer.
 */
class Indexer
{
private:
  FileIndexingArbiter& m_fileIndexingArbiter;
  FileIdentificator& m_fileIdentificator;
  std::unique_ptr<SymbolCollector> m_symbolCollector;
  std::unique_ptr<IdxrDiagnosticConsumer> m_diagnosticConsumer;
  clang::ASTContext* mAstContext = nullptr;
  std::shared_ptr<clang::Preprocessor> m_pp;
  std::unique_ptr<TranslationUnitIndex> m_index;
  std::map<clang::FileID, bool> m_ShouldIndexFileCache;
  std::map<clang::FileID, cppscanner::FileID> m_FileIdCache;

public:

  explicit Indexer(FileIndexingArbiter& fileIndexingArbiter);
  ~Indexer();

  FileIdentificator& fileIdentificator();

  clang::DiagnosticConsumer* getOrCreateDiagnosticConsumer();

  TranslationUnitIndex* getCurrentIndex() const;
  void resetCurrentIndex();

  bool shouldIndexFile(clang::FileID fileId);
  bool ShouldTraverseDecl(const clang::Decl* decl);
  cppscanner::FileID getFileID(clang::FileID clangFileId);
  std::pair<FileID, FilePosition> convert(const clang::SourceLocation& loc);
  std::pair<FileID, FilePosition> convert(clang::FileID fileId, const clang::SourceLocation& loc);
  clang::FileID getClangFileID(const cppscanner::FileID id);
  clang::ASTContext* getAstContext() const;
  clang::Preprocessor* getPreprocessor() const;
  bool initialized() const;

  // clang::index::IndexDataConsumer interface
  void initialize(clang::ASTContext& Ctx);
  void setPreprocessor(std::shared_ptr<clang::Preprocessor> PP);
  bool handleDeclOccurrence(const clang::Decl* D, clang::index::SymbolRoleSet Roles,
    llvm::ArrayRef<clang::index::SymbolRelation> relations,
    clang::SourceLocation Loc, clang::index::IndexDataConsumer::ASTNodeInfo ASTNode);
  bool handleMacroOccurrence(const clang::IdentifierInfo* Name,
    const clang::MacroInfo* MI, clang::index::SymbolRoleSet Roles,
    clang::SourceLocation Loc);
  bool handleModuleOccurrence(const clang::ImportDecl* ImportD,
    const clang::Module* Mod, clang::index::SymbolRoleSet Roles,
    clang::SourceLocation Loc);
  void finish();
  // end clang::index::IndexDataConsumer

  // clang::DiagnosticConsumer interface
  void HandleDiagnostic(clang::DiagnosticsEngine::Level dlvl, const clang::Diagnostic& dinfo);
  // end clang::DiagnosticConsumer

  SymbolID getMacroSymbolIdFromCache(const clang::MacroInfo* macroInfo) const;

protected:
  SymbolCollector& symbolCollector() const;
  clang::SourceManager& getSourceManager() const;

protected:
  void processRelations(std::pair<const clang::Decl*, IndexerSymbol*> declAndSymbol, clang::SourceLocation refLocation, llvm::ArrayRef<clang::index::SymbolRelation> relations);
  void indexPreprocessingRecord(clang::Preprocessor& pp);
  void recordSymbolDeclarations();
  void recordSymbolDeclaration(const IndexerSymbol& symbol, const clang::Decl& declaration);
};

} // namespace cppscanner

#endif // CPPSCANNER_INDEXER_H

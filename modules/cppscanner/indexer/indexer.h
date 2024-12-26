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

namespace clang
{
class InclusionDirective;
class MacroDefinitionRecord;
class MacroExpansion;
} // namespace clang

namespace cppscanner
{

class FileIndexingArbiter;
class FileIdentificator;

class Indexer;

/**
 * \brief a class for collecting C++ symbols while indexing a translation unit
 * 
 * This class creates an IndexerSymbol for each C++ symbol encountered while 
 * indexing a translation unit. Macros are also handled by this class.
 * 
 * Because all declarations go through this class, it is also used by the 
 * Indexer class for post-processing purposes: the location of some declarations 
 * that went thought the SymbolCollector are recorded in the output TranslationUnitIndex.
 */
class SymbolCollector
{
private:
  Indexer& m_indexer;
  std::map<const clang::Decl*, SymbolID> m_symbolIdCache;
  std::map<const clang::MacroInfo*, SymbolID> m_macroIdCache;
  std::map<const clang::Module*, SymbolID> m_moduleIdCache;

public:
  explicit SymbolCollector(Indexer& idxr);

  void reset();

  IndexerSymbol* process(const clang::Decl* decl);
  IndexerSymbol* process(const clang::IdentifierInfo* name, const clang::MacroInfo* macroInfo);
  IndexerSymbol* process(const clang::Module* moduleInfo);

  SymbolID getMacroSymbolIdFromCache(const clang::MacroInfo* macroInfo) const;

  const std::map<const clang::Decl*, SymbolID>& declarations() const;

protected:
  std::string getDeclSpelling(const clang::Decl* decl);
  void fillSymbol(IndexerSymbol& symbol, const clang::Decl* decl);
  void fillSymbol(IndexerSymbol& symbol,const clang::IdentifierInfo* name, const clang::MacroInfo* macroInfo);
  void fillSymbol(IndexerSymbol& symbol, const clang::Module* moduleInfo);
  IndexerSymbol* getParentSymbol(const IndexerSymbol& symbol, const clang::Decl* decl);
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
 * \brief class for collecting information from a PreprocessingRecord
 * 
 * This class is used by the Indexer class for listing files that were #included 
 * in the translation unit and for checking if macros were used as header guards.
 */
class PreprocessingRecordIndexer
{
private:
  Indexer& m_indexer;
public:
  explicit PreprocessingRecordIndexer(Indexer& idxr);

  void process(clang::PreprocessingRecord* ppRecord);

protected:
  clang::SourceManager& getSourceManager() const;
  bool shouldIndexFile(const clang::FileID fileId) const;
  TranslationUnitIndex& currentIndex() const;
  cppscanner::FileID idFile(const clang::FileID& fileId) const;
  cppscanner::FileID idFile(const std::string& filePath) const;

protected:
  void process(clang::InclusionDirective& inclusionDirective);
  void process(clang::MacroDefinitionRecord& mdr);
  void process(clang::MacroExpansion& macroExpansion);
};

/**
 * \brief class for receiving indexing data from clang
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
 * Compiler diagnostics are not handled by this class but rather
 * by the IdxrDiagnosticConsumer class.
 */
class Indexer : public clang::index::IndexDataConsumer
{
private:
  FileIndexingArbiter& m_fileIndexingArbiter;
  FileIdentificator& m_fileIdentificator;
  SymbolCollector m_symbolCollector;
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

  SymbolCollector& symbolCollector();
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

  void initialize(clang::ASTContext& Ctx) override;
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
  void finish() override;

protected:
  friend IdxrDiagnosticConsumer;
  clang::SourceManager& getSourceManager() const;

protected:
  void processRelations(std::pair<const clang::Decl*, IndexerSymbol*> declAndSymbol, clang::SourceLocation refLocation, llvm::ArrayRef<clang::index::SymbolRelation> relations);
  void indexPreprocessingRecord(clang::Preprocessor& pp);
  void recordSymbolDeclarations();
  void recordSymbolDeclaration(const IndexerSymbol& symbol, const clang::Decl& declaration);
};

} // namespace cppscanner

#endif // CPPSCANNER_INDEXER_H

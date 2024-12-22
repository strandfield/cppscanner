// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_SNAPSHOTWRITER_H
#define CPPSCANNER_SNAPSHOTWRITER_H

#include "cppscanner/database/database.h"
#include "cppscanner/database/transaction.h"

#include "cppscanner/index/baseof.h"
#include "cppscanner/index/declaration.h"
#include "cppscanner/index/diagnostic.h"
#include "cppscanner/index/file.h"
#include "cppscanner/index/include.h"
#include "cppscanner/index/override.h"
#include "cppscanner/index/refarg.h"
#include "cppscanner/index/reference.h"
#include "cppscanner/index/symbol.h"

#include <filesystem>
#include <map>
#include <memory>
#include <vector>

namespace cppscanner
{

class IndexerSymbol;

/**
 * \brief class for writing a snapshot of a C++ program stored as a SQLite database
 * 
 * This class is used by the Scanner class to produce a snapshot by aggregating one 
 * or more TranslationUnitIndex.
 */
class SnapshotWriter
{
public:
  SnapshotWriter();
  SnapshotWriter(const SnapshotWriter&) = delete;
  SnapshotWriter(SnapshotWriter&&);
  ~SnapshotWriter();

  explicit SnapshotWriter(const std::filesystem::path& p);

  bool open();
  bool open(const std::filesystem::path& p);
  bool isOpen() const;

  Database& database() const;

  static constexpr int DatabaseSchemaVersion = 0;

  class Path
  {
  private:
    std::string m_path;
  public:
    explicit Path(std::string p) : m_path(SnapshotWriter::normalizedPath(std::move(p))) { }
    const std::string& str() const { return m_path; }
  };

  static std::string normalizedPath(std::string p);

  void setProperty(const std::string& key, const std::string& value);
  void setProperty(const std::string& key, bool value);
  void setProperty(const std::string& key, int value);
  void setProperty(const std::string& key, const char* value);
  void setProperty(const std::string& key, const Path& path);

  void insertFilePaths(const std::vector<File>& files);
  void insertFiles(const std::vector<File>& files);

  void insertIncludes(const std::vector<Include>& includes);
  void insertSymbols(const std::vector<const IndexerSymbol*>& symbols);
  void updateSymbolsFlags(const std::vector<const IndexerSymbol*>& symbols);
  void insertBaseOfs(const std::vector<BaseOf>& bofs);
  void insertOverrides(const std::vector<Override>& overrides);
  void insertDiagnostics(const std::vector<Diagnostic>& diagnostics);
  void insert(const std::vector<ArgumentPassedByReference>& refargs);
  void insert(const std::vector<SymbolReference>& refs);
  void insert(const std::vector<SymbolDeclaration>& declarations);

  void insert(const std::map<SymbolID, EnumConstantInfo>& infomap);
  void insert(const std::map<SymbolID, EnumInfo>& infomap);
  void insert(const std::map<SymbolID, FunctionInfo>& infomap);
  void insert(const std::map<SymbolID, MacroInfo>& infomap);
  void insert(const std::map<SymbolID, NamespaceAliasInfo>& infomap);
  void insert(const std::map<SymbolID, ParameterInfo>& infomap);
  void insert(const std::map<SymbolID, VariableInfo>& infomap);

  std::vector<Include> loadAllIncludesInFile(FileID fid);
  void removeAllIncludesInFile(FileID fid);

  std::vector<SymbolReference> loadSymbolReferencesInFile(FileID fid);
  void removeAllSymbolReferencesInFile(FileID fid);

  std::vector<Diagnostic> loadDiagnosticsInFile(FileID fid);
  void removeAllDiagnosticsInFile(FileID fid);

  std::vector<SymbolDeclaration> loadDeclarationsInFile(FileID fid);
  void removeAllDeclarationsInFile(FileID fid);

  void beginTransaction();
  void endTransaction();

private:
  std::filesystem::path m_database_path;
  std::unique_ptr<Database> m_database;
  std::unique_ptr<sql::Transaction> m_transaction;
};

namespace snapshot
{

const char* db_init_statements();

[[deprecated]] void insertFiles(SnapshotWriter& snapshot, const std::vector<File>& files);
[[deprecated]] void insertSymbolReferences(SnapshotWriter& snapshot, const std::vector<SymbolReference>& references);

} // namespace snapshot

} // namespace cppscanner

#endif // CPPSCANNER_SNAPSHOTWRITER_H

// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_SNAPSHOT_H
#define CPPSCANNER_SNAPSHOT_H

#include "cppscanner/database/database.h"

#include "cppscanner/index/baseof.h"
#include "cppscanner/index/diagnostic.h"
#include "cppscanner/index/file.h"
#include "cppscanner/index/include.h"
#include "cppscanner/index/override.h"
#include "cppscanner/index/reference.h"
#include "cppscanner/index/symbol.h"

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace sql
{
class Like;
} // namespace sql

namespace cppscanner
{

class IndexerSymbol;

/**
 * \brief a snapshot of a C++ program stored as a SQLite database
 *
 * Use open() or create() to get a valid Snapshot object.
 */
class Snapshot
{
public:
  Snapshot() = delete;
  Snapshot(const Snapshot&) = delete;
  Snapshot(Snapshot&&);
  ~Snapshot();

  explicit Snapshot(Database db);

  Database& database() const;

  static Snapshot open(const std::filesystem::path& p);
  static Snapshot create(const std::filesystem::path& p);

  class Path
  {
  private:
    std::string m_path;
  public:
    explicit Path(std::string p) : m_path(Snapshot::normalizedPath(std::move(p))) { }
    const std::string& str() const { return m_path; }
  };

  static std::string normalizedPath(std::string p);

  void setProperty(const std::string& key, const std::string& value);
  void setProperty(const std::string& key, bool value);
  void setProperty(const std::string& key, const char* value);
  void setProperty(const std::string& key, const Path& path);

  void insertFilePaths(const std::vector<File>& files);
  void insertIncludes(const std::vector<Include>& includes);
  void insertSymbols(const std::vector<const IndexerSymbol*>& symbols);
  void insertBaseOfs(const std::vector<BaseOf>& bofs);
  void insertOverrides(const std::vector<Override>& overrides);
  void insertDiagnostics(const std::vector<Diagnostic>& diagnostics);

  std::vector<Include> loadAllIncludesInFile(FileID fid);
  void removeAllIncludesInFile(FileID fid);

  std::vector<SymbolReference> loadSymbolReferencesInFile(FileID fid);
  void removeAllSymbolReferencesInFile(FileID fid);

  std::vector<Diagnostic> loadDiagnosticsInFile(FileID fid);
  void removeAllDiagnosticsInFile(FileID fid);

  // for testing purposes
  std::vector<File> getFiles() const;
  std::vector<Include> getIncludedFiles(FileID fid) const;

  std::vector<SymbolRecord> findSymbolsByName(const std::string& name) const;
  std::vector<SymbolRecord> findSymbolsByName(const sql::Like& name) const;
  SymbolRecord getSymbolByName(const std::string& name, SymbolID parentID = {}) const;
  SymbolRecord getSymbolByName(const sql::Like& name, SymbolID parentID = {}) const;
  SymbolRecord getSymbol(const std::vector<std::string>& qualifiedName) const;
  std::vector<SymbolRecord> getSymbols(SymbolID parentID) const;
  std::vector<SymbolRecord> getSymbols(SymbolID parentID, SymbolKind kind) const;
  NamespaceAliasRecord getNamespaceAliasRecord(const std::string& name) const;
  std::vector<ParameterRecord> getParameters(SymbolID symbolId, SymbolKind parameterKind) const;
  std::vector<ParameterRecord> getFunctionParameters(SymbolID functionId, SymbolKind kind = SymbolKind::Parameter) const;
  VariableRecord getField(SymbolID classId, const std::string& name) const;
  std::vector<VariableRecord> getFields(SymbolID classId) const;
  std::vector<VariableRecord> getStaticProperties(SymbolID classId) const;

  std::vector<BaseOf> getBasesOf(SymbolID classID) const;
  std::vector<Override> getOverridesOf(SymbolID methodID) const;
  std::vector<SymbolReference> findReferences(SymbolID symbolID);

private:
  std::unique_ptr<Database> m_database;
};

namespace snapshot
{

const char* db_init_statements();

void insertFiles(Snapshot& snapshot, const std::vector<File>& files);
void insertSymbolReferences(Snapshot& snapshot, const std::vector<SymbolReference>& references);

} // namespace snapshot

} // namespace cppscanner

#endif // CPPSCANNER_SNAPSHOT_H

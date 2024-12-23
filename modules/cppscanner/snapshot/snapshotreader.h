// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_SNAPSHOTREADER_H
#define CPPSCANNER_SNAPSHOTREADER_H

#include "snapshot.h"

#include "cppscanner/database/database.h"

#include <filesystem>
#include <initializer_list>
#include <memory>
#include <vector>

namespace cppscanner
{

/**
 * \brief helper class for reading snapshots of C++ program produced by the scanner
 * 
 * The primary use of this class is for writing tests.
 * */
class SnapshotReader
{
public:
  SnapshotReader();
  SnapshotReader(const SnapshotReader&) = delete;
  SnapshotReader(SnapshotReader&&);
  ~SnapshotReader();

  explicit SnapshotReader(const std::filesystem::path& p);
  explicit SnapshotReader(Database db);

  bool open();
  bool open(const std::filesystem::path& databasePath);
  bool isOpen() const;
  void close();
  bool reopen();

  Database& database() const;

  Snapshot::Properties readProperties() const;

  std::vector<File> getFiles(bool fetchContent = false) const;
  std::vector<Include> getIncludes() const;
  std::vector<Include> getIncludedFiles(FileID fid) const;
  std::vector<ArgumentPassedByReference> getArgumentsPassedByReference() const;
  std::vector<ArgumentPassedByReference> getArgumentsPassedByReference(FileID file) const;
  std::vector<SymbolDeclaration> getSymbolDeclarations() const;
  std::vector<SymbolDeclaration> getSymbolDeclarations(SymbolID symbolId) const;

  std::vector<SymbolRecord> getSymbolsByName(const std::string& name) const;
  SymbolRecord getChildSymbolByName(const std::string& name, SymbolID parentID) const;
  SymbolRecord getSymbolByName(const std::vector<std::string>& qualifiedName) const;
  SymbolRecord getSymbolByName(std::initializer_list<std::string>&& qualifiedName) const;
  SymbolRecord getSymbolById(SymbolID id) const;
  SymbolRecord getSymbolByName(const std::string& name) const;
  std::vector<SymbolRecord> getChildSymbols(SymbolID parentID) const;
  std::vector<SymbolRecord> getChildSymbols(SymbolID parentID, SymbolKind kind) const;
  NamespaceAliasRecord getNamespaceAliasRecord(const std::string& name) const;
  std::vector<ParameterRecord> getParameters(SymbolID symbolId, SymbolKind parameterKind) const;
  std::vector<ParameterRecord> getFunctionParameters(SymbolID functionId, SymbolKind kind = SymbolKind::Parameter) const;
  VariableRecord getField(SymbolID classId, const std::string& name) const;
  std::vector<VariableRecord> getFields(SymbolID classId) const;
  std::vector<VariableRecord> getStaticProperties(SymbolID classId) const;

  std::vector<BaseOf> getBases() const;
  std::vector<BaseOf> getBasesOf(SymbolID classID) const;
  std::vector<Override> getOverrides() const;
  std::vector<Override> getOverridesOf(SymbolID methodID) const;

  std::vector<SymbolReference> getSymbolReferences() const;
  std::vector<SymbolReference> findReferences(SymbolID symbolID) const;

  std::vector<Diagnostic> getDiagnostics() const;

  SnapshotReader& operator=(SnapshotReader&&) = default;

private:
  std::filesystem::path m_database_path;
  std::unique_ptr<Database> m_database;
};

void sort(std::vector<SymbolReference>& refs);

} // namespace cppscanner

#endif // CPPSCANNER_SNAPSHOTREADER_H

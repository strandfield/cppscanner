// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#include "snapshotreader.h"

#include "symbolrecorditerator.h"

#include "cppscanner/database/readrows.h"

#include <algorithm>
#include <cassert>

namespace cppscanner
{

SnapshotReader::SnapshotReader()
{

}

SnapshotReader::SnapshotReader(SnapshotReader&&) = default;
SnapshotReader::~SnapshotReader() = default;

SnapshotReader::SnapshotReader(const std::filesystem::path& databasePath)
{
  if (!open(databasePath))
  {
    throw std::runtime_error("could not open snapshot");
  }
}

SnapshotReader::SnapshotReader(Database db) : 
  m_database(std::make_unique<Database>(std::move(db)))
{
  if (!m_database->good())
    throw std::runtime_error("snapshot constructor expects a good() database");
}

bool SnapshotReader::open()
{
  if (!reopen())
  {
    return false;
  }

  // check that the database is likely a snapshot produced by the scanner
  {
    sql::Statement stmt{
      *m_database,
      "SELECT name FROM sqlite_master WHERE type='table' AND name='info'"
    };

    if (!stmt.fetchNextRow())
    {
      stmt.finalize();
      m_database.reset();
      return false;
    }
  }

  return true;
}

bool SnapshotReader::open(const std::filesystem::path& databasePath)
{
  m_database_path = databasePath;
  return open();
}

bool SnapshotReader::isOpen() const
{
  return m_database != nullptr;
}


void SnapshotReader::close()
{
  m_database.reset();
}

bool SnapshotReader::reopen()
{
  if (isOpen())
  {
    return true;
  }

  m_database = std::make_unique<Database>();
  m_database->open(m_database_path);

  if (!m_database->good())
  {
    m_database.reset();
    return false;
  }

  return true;
}

/**
 * \brief returns the database associated with the snapshot
 */
Database& SnapshotReader::database() const
{
  assert(isOpen());
  return *m_database;
}

inline static Snapshot::Property readProperty(sql::Statement& row)
{
  Snapshot::Property res;
  res.name = row.column(0);
  res.value = row.column(1);
  return res;
}

Snapshot::Properties SnapshotReader::readProperties() const
{
  sql::Statement stmt{ 
    database(),
    "SELECT key, value FROM info"
  };

  Snapshot::Properties result;
  sql::readRowsConverted(stmt, &readProperty, [&result](const Snapshot::Property& item) {
    result[item.name] = item.value;
    });
  return result;
}

inline static File readFileNoContent(sql::Statement& row)
{
  File f;
  f.id = row.columnInt(0);
  f.path = row.column(1);
  f.sha1 = row.column(2);
  return f;
}

inline static File readFileWithContent(sql::Statement& row)
{
  File f = readFileNoContent(row);
  f.content = row.column(3);
  return f;
}

std::vector<File> SnapshotReader::getFiles(bool fetchContent) const
{
  if (fetchContent)
  {
    sql::Statement stmt{ 
      database(),
      "SELECT id, path, sha1, content FROM file"
    };

    return sql::readRowsAsVector<File>(stmt, readFileWithContent);
  }
  else
  {
    sql::Statement stmt{ 
      database(),
      "SELECT id, path, sha1 FROM file"
    };

    return sql::readRowsAsVector<File>(stmt, readFileNoContent);
  }
}

inline static Include readInclude(sql::Statement& row)
{
  Include i;
  i.fileID = row.columnInt(0);
  i.includedFileID = row.columnInt(1);
  i.line = row.columnInt(2);
  return i;
}

std::vector<Include> SnapshotReader::getIncludes() const
{
  sql::Statement stmt { 
    database(),
    "SELECT file_id, included_file_id, line FROM include"
  };

  return sql::readRowsAsVector<Include>(stmt, readInclude);
}

std::vector<Include> SnapshotReader::getIncludedFiles(FileID fid) const
{
  sql::Statement stmt { 
    database(),
    "SELECT file_id, included_file_id, line FROM include WHERE file_id = ?"
  };

  stmt.bind(1, (int)fid);

  return sql::readRowsAsVector<Include>(stmt, readInclude);
}

inline static ArgumentPassedByReference readArgumentPassedByReference(sql::Statement& row)
{
  ArgumentPassedByReference ret;
  ret.fileID = row.columnInt(0);
  ret.position = FilePosition(row.columnInt(1), row.columnInt(2));
  return ret;
}

std::vector<ArgumentPassedByReference> SnapshotReader::getArgumentsPassedByReference() const
{
  sql::Statement stmt { 
    database(),
    "SELECT file_id, line, column FROM argumentPassedByReference"
  };

  return sql::readRowsAsVector<ArgumentPassedByReference>(stmt, readArgumentPassedByReference);
}

std::vector<ArgumentPassedByReference> SnapshotReader::getArgumentsPassedByReference(FileID file) const
{
  sql::Statement stmt { 
    database(),
    "SELECT file_id, line, column FROM argumentPassedByReference WHERE file_id = ?"
  };

  stmt.bind(1, (int)file);

  return sql::readRowsAsVector<ArgumentPassedByReference>(stmt, readArgumentPassedByReference);
}

inline static SymbolDeclaration readSymbolDeclaration(sql::Statement& row)
{
  SymbolDeclaration ret;
  ret.symbolID = SymbolID::fromRawID(row.columnInt64(0));
  ret.fileID = row.columnInt(1);
  ret.startPosition = FilePosition::fromBits(row.columnInt(2));
  ret.endPosition = FilePosition::fromBits(row.columnInt(3));
  ret.isDefinition = (row.columnInt(4) != 0);
  return ret;
}

std::vector<SymbolDeclaration> SnapshotReader::getSymbolDeclarations() const
{
  sql::Statement stmt { 
    database(),
    "SELECT symbol_id, file_id, startPosition, endPosition, isDefinition FROM symbolDeclaration"
  };

  return sql::readRowsAsVector<SymbolDeclaration>(stmt, readSymbolDeclaration);
}

std::vector<SymbolDeclaration> SnapshotReader::getSymbolDeclarations(SymbolID symbolId) const
{
  sql::Statement stmt { 
    database(),
    "SELECT symbol_id, file_id, startPosition, endPosition, isDefinition FROM symbolDeclaration WHERE symbol_id = ?"
  };

  stmt.bind(1, symbolId.rawID());

  return sql::readRowsAsVector<SymbolDeclaration>(stmt, readSymbolDeclaration);
}

std::vector<SymbolRecord> SnapshotReader::getSymbolsByName(const std::string& name) const
{
  return fetchAll(*this, SymbolRecordFilter().withName(name));
}

SymbolRecord SnapshotReader::getChildSymbolByName(const std::string& name, SymbolID parentID) const
{
  if (parentID == SymbolID()) {
    std::vector<SymbolRecord> symbols = getSymbolsByName(name);

    if (symbols.size() != 1) {
      throw std::runtime_error("could not find unique symbol with given name: " + name);
    }

    return symbols.front();
  } else {
    return getRecord(*this, SymbolRecordFilter().withName(name).withParent(parentID));
  }
}

SymbolRecord SnapshotReader::getSymbolByName(const std::vector<std::string>& qualifiedName) const
{
  SymbolRecord s;

  for (const std::string& name : qualifiedName)
  {
    s = getChildSymbolByName(name, s.id);
  }

  return s;
}

SymbolRecord SnapshotReader::getSymbolByName(std::initializer_list<std::string>&& qualifiedName) const
{
  return getSymbolByName(std::vector<std::string>(qualifiedName.begin(), qualifiedName.end()));
}

SymbolRecord SnapshotReader::getSymbolById(SymbolID id) const
{
  return getRecord(*this, SymbolRecordFilter().withId(id));
}

SymbolRecord SnapshotReader::getSymbolByName(const std::string& name) const
{
  return getChildSymbolByName(name, SymbolID());
}

std::vector<SymbolRecord> SnapshotReader::getChildSymbols(SymbolID parentID) const
{
  return fetchAll(*this, SymbolRecordFilter().withParent(parentID));
}

std::vector<SymbolRecord> SnapshotReader::getChildSymbols(SymbolID parentID, SymbolKind kind) const
{
  return fetchAll(*this, SymbolRecordFilter().withParent(parentID).ofKind(kind));
}

NamespaceAliasRecord SnapshotReader::getNamespaceAliasRecord(const std::string& name) const
{
  return getRecord<NamespaceAliasRecord>(*this, SymbolRecordFilter().withName(name));
}

std::vector<ParameterRecord> SnapshotReader::getParameters(SymbolID symbolId, SymbolKind parameterKind) const
{
  return fetchAll<ParameterRecord>(*this, SymbolRecordFilter().ofKind(parameterKind).withParent(symbolId));
}

std::vector<ParameterRecord> SnapshotReader::getFunctionParameters(SymbolID functionId, SymbolKind kind) const
{
  return getParameters(functionId, kind);
}

VariableRecord SnapshotReader::getField(SymbolID classId, const std::string& name) const
{
  return getRecord<VariableRecord>(*this, SymbolRecordFilter().ofKind(SymbolKind::Field).withName(name).withParent(classId));
}

std::vector<VariableRecord> SnapshotReader::getFields(SymbolID classId) const
{
  return fetchAll<VariableRecord>(*this, SymbolRecordFilter().ofKind(SymbolKind::Field).withParent(classId));
}

std::vector<VariableRecord> SnapshotReader::getStaticProperties(SymbolID classId) const
{
  return fetchAll<VariableRecord>(*this, SymbolRecordFilter().ofKind(SymbolKind::StaticProperty).withParent(classId));
}

SymbolID sqlColumnAsSymbolID(sql::Statement& row, int col)
{
  return SymbolID::fromRawID(row.columnInt64(col));
}

AccessSpecifier sqlColumnAsAccessSpecifier(sql::Statement& row, int col)
{
  return static_cast<AccessSpecifier>(row.columnInt(col));
}

std::vector<BaseOf> SnapshotReader::getBases() const
{
  sql::Statement stmt{ 
    database(),
    "SELECT baseClassID, derivedClassID, access FROM baseOf"
  };

  auto rr = [](sql::Statement& row) -> BaseOf {
    BaseOf bof;
    bof.baseClassID = sqlColumnAsSymbolID(row, 0);
    bof.derivedClassID = sqlColumnAsSymbolID(row, 1);
    bof.accessSpecifier = sqlColumnAsAccessSpecifier(row, 2);
    return bof;
    };

  return sql::readRowsAsVector<BaseOf>(stmt, rr);
}

std::vector<BaseOf> SnapshotReader::getBasesOf(SymbolID classID) const
{
  sql::Statement stmt{ 
    database(),
    "SELECT baseClassID, access FROM baseOf WHERE derivedClassID = ?"
  };

  stmt.bind(1, classID.rawID());

  auto rr = [classID](sql::Statement& row) -> BaseOf {
    BaseOf bof;
    bof.derivedClassID = classID;
    bof.baseClassID = sqlColumnAsSymbolID(row, 0);
    bof.accessSpecifier = sqlColumnAsAccessSpecifier(row, 1);
    return bof;
    };

  return sql::readRowsAsVector<BaseOf>(stmt, rr);
}

std::vector<Override> SnapshotReader::getOverrides() const
{
  sql::Statement stmt{ 
    database(),
    "SELECT baseMethodID, overrideMethodID FROM override"
  };

  auto rr = [](sql::Statement& row) -> Override {
    Override ov;
    ov.baseMethodID = sqlColumnAsSymbolID(row, 0);
    ov.overrideMethodID = sqlColumnAsSymbolID(row, 1);
    return ov;
    };

  return sql::readRowsAsVector<Override>(stmt, rr);
}

std::vector<Override> SnapshotReader::getOverridesOf(SymbolID methodID) const
{
  sql::Statement stmt{ 
    database(),
    "SELECT overrideMethodID FROM override WHERE baseMethodID = ?"
  };

  stmt.bind(1, methodID.rawID());

  auto rr = [methodID](sql::Statement& row) -> Override {
    Override ov;
    ov.baseMethodID = methodID;
    ov.overrideMethodID = sqlColumnAsSymbolID(row, 0);
    return ov;
    };

  return sql::readRowsAsVector<Override>(stmt, rr);
}

inline static SymbolReference readSymbolReference(sql::Statement& row)
{
  SymbolReference r;
  r.symbolID = SymbolID::fromRawID(row.columnInt64(0));
  r.fileID = row.columnInt(1);
  r.position = FilePosition(row.columnInt(2), row.columnInt(3));
  r.referencedBySymbolID = SymbolID::fromRawID(row.columnInt64(4));
  r.flags = row.columnInt(5);
  return r;
}

std::vector<SymbolReference> SnapshotReader::getSymbolReferences() const
{
  sql::Statement stmt{ 
    database(),
    "SELECT symbol_id, file_id, line, col, parent_symbol_id, flags FROM symbolReference"
  };

  return sql::readRowsAsVector<SymbolReference>(stmt, readSymbolReference);
}

std::vector<SymbolReference> SnapshotReader::findReferences(SymbolID symbolID) const
{
  sql::Statement stmt{ 
    database(),
    "SELECT symbol_id, file_id, line, col, parent_symbol_id, flags FROM symbolReference WHERE symbol_id = ?"
  };

  stmt.bind(1, symbolID.rawID());

  return sql::readRowsAsVector<SymbolReference>(stmt, readSymbolReference);
}

inline static Diagnostic readDiagnostic(sql::Statement& row)
{
  Diagnostic r;
  r.level = static_cast<DiagnosticLevel>(row.columnInt(0));
  r.fileID = row.columnInt(1);
  r.position = FilePosition(row.columnInt(2), row.columnInt(3));
  r.message = row.column(4);
  return r;
}

std::vector<Diagnostic> SnapshotReader::getDiagnostics() const
{
  sql::Statement stmt{ 
    database(),
    "SELECT level, fileID, line, column, message FROM diagnostic"
  };

  return sql::readRowsAsVector<Diagnostic>(stmt, readDiagnostic);
}

void sort(std::vector<SymbolReference>& refs)
{
  std::sort(refs.begin(), refs.end(), [](const SymbolReference& a, const SymbolReference& b) {
    return std::forward_as_tuple(a.fileID, a.position) < std::forward_as_tuple(b.fileID, b.position);
    });
}

} // namespace cppscanner

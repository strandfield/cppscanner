// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#include "snapshotreader.h"

#include "symbolrecorditerator.h"

#include "cppscanner/database/readrows.h"

namespace cppscanner
{

SnapshotReader::SnapshotReader(SnapshotReader&&) = default;
SnapshotReader::~SnapshotReader() = default;

SnapshotReader::SnapshotReader(const std::filesystem::path& databasePath)
  : m_database(std::make_unique<Database>())
{
  m_database->open(databasePath);

  if (!m_database->good())
    throw std::runtime_error("snapshot constructor expects a good() database");
}

SnapshotReader::SnapshotReader(Database db) : 
  m_database(std::make_unique<Database>(std::move(db)))
{
  if (!m_database->good())
    throw std::runtime_error("snapshot constructor expects a good() database");
}

/**
 * \brief returns the database associated with the snapshot
 */
Database& SnapshotReader::database() const
{
  return *m_database;
}

inline static File readFileIdAndPath(sql::Statement& row)
{
  File f;
  f.id = row.columnInt(0);
  f.path = row.column(1);
  return f;
}

std::vector<File> SnapshotReader::getFiles() const
{
  sql::Statement stmt{ 
    database(),
    "SELECT id, path FROM file"
  };

  return sql::readRowsAsVector<File>(stmt, readFileIdAndPath);
}

inline static Include readInclude(sql::Statement& row)
{
  Include i;
  i.fileID = row.columnInt(0);
  i.includedFileID = row.columnInt(1);
  i.line = row.columnInt(2);
  return i;
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

std::vector<SymbolReference> SnapshotReader::findReferences(SymbolID symbolID)
{
  sql::Statement stmt{ 
    database(),
    "SELECT symbol_id, file_id, line, col, parent_symbol_id, flags FROM symbolReference WHERE symbol_id = ?"
  };

  stmt.bind(1, symbolID.rawID());

  return sql::readRowsAsVector<SymbolReference>(stmt, readSymbolReference);
}

} // namespace cppscanner

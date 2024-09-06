// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#include "snapshot.h"

#include "indexersymbol.h"

#include "cppscanner/database/sql.h"
#include "cppscanner/database/transaction.h"

#include "cppscanner/index/relation.h"
#include "cppscanner/index/symbol.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>

namespace cppscanner
{

static const char* SQL_CREATE_STATEMENTS = R"(
BEGIN TRANSACTION;

CREATE TABLE "info" (
  "key" TEXT NOT NULL,
  "value" TEXT NOT NULL
);

CREATE TABLE "accessSpecifier" (
  "value"  INTEGER NOT NULL PRIMARY KEY UNIQUE,
  "name"   TEXT NOT NULL
);


CREATE TABLE "file" (
  "id"      INTEGER NOT NULL PRIMARY KEY UNIQUE,
  "path"    TEXT NOT NULL,
  "content" TEXT
);

CREATE TABLE "include" (
  "file_id"                       INTEGER NOT NULL,
  "line"                          INTEGER NOT NULL,
  "included_file_id"              INTEGER NOT NULL,
  FOREIGN KEY("file_id")          REFERENCES "file"("id"),
  FOREIGN KEY("included_file_id") REFERENCES "file"("id"),
  UNIQUE(file_id, line)
);

CREATE TABLE "symbolKind" (
  "id"   INTEGER NOT NULL PRIMARY KEY UNIQUE,
  "name" TEXT NOT NULL
);

CREATE TABLE "symbolFlag" (
  "name"   TEXT NOT NULL,
  "value"  INTEGER NOT NULL
);

CREATE TABLE "symbol" (
  "id"                INTEGER NOT NULL PRIMARY KEY UNIQUE,
  "kind"              INTEGER NOT NULL,
  "parent"            INTEGER,
  "name"              TEXT NOT NULL,
  "flags"             INTEGER NOT NULL DEFAULT 0,
  "parameterIndex"    INTEGER,
  "type"              TEXT,
  "value"             TEXT,
  FOREIGN KEY("kind") REFERENCES "symbolKind"("id")
);

CREATE TABLE "symbolReferenceFlag" (
  "name"   TEXT NOT NULL,
  "value"  INTEGER NOT NULL
);

CREATE TABLE "symbolReference" (
  "symbol_id"                     INTEGER NOT NULL,
  "file_id"                       INTEGER NOT NULL,
  "line"                          INTEGER NOT NULL,
  "col"                           INTEGER NOT NULL,
  "parent_symbol_id"              INTEGER,
  "flags"                         INTEGER NOT NULL DEFAULT 0,
  FOREIGN KEY("symbol_id")        REFERENCES "symbol"("id"),
  FOREIGN KEY("file_id")          REFERENCES "file"("id"),
  FOREIGN KEY("parent_symbol_id") REFERENCES "symbol"("id")
);

CREATE TABLE "relationPredicate" (
  "id"   INTEGER NOT NULL PRIMARY KEY UNIQUE,
  "name" TEXT NOT NULL
);

CREATE TABLE "baseOf" (
  "baseClassID"                  INTEGER NOT NULL,
  "derivedClassID"               INTEGER NOT NULL,
  "access"                       INTEGER NOT NULL,
  FOREIGN KEY("baseClassID")     REFERENCES "symbol"("id"),
  FOREIGN KEY("derivedClassID")  REFERENCES "symbol"("id"),
  FOREIGN KEY("access")          REFERENCES "accessSpecifier"("value"),
  UNIQUE(baseClassID, derivedClassID)
);

CREATE TABLE "override" (
  "overrideMethodID"               INTEGER NOT NULL PRIMARY KEY UNIQUE,
  "baseMethodID"                   INTEGER NOT NULL,
  FOREIGN KEY("overrideMethodID")  REFERENCES "symbol"("id"),
  FOREIGN KEY("baseMethodID")      REFERENCES "symbol"("id")
);

CREATE TABLE "diagnosticLevel" (
  "value"  INTEGER NOT NULL PRIMARY KEY UNIQUE,
  "name"   TEXT NOT NULL
);

CREATE TABLE "diagnostic" (
  "level"                INTEGER NOT NULL,
  "fileID"               INTEGER NOT NULL,
  "line"                 INTEGER NOT NULL,
  "column"               INTEGER NOT NULL,
  "message"              TEXT NOT NULL,
  FOREIGN KEY("fileID")  REFERENCES "file"("id")
);

CREATE VIEW symbolDefinition (symbol_id, file_id, line, col, flags) AS
  SELECT symbol_id, file_id, line, col, flags
  FROM symbolReference WHERE flags & 2;

COMMIT;
)";

#ifdef _WIN32
std::string NormalizedPath(std::string path)
{
  if (path.length() >= 2 && path.at(1) == ':') {
    path[1] = std::tolower(path[0]);
    path[0] = '/';
  }
  for (char& c : path) {
    if (c == '\\') {
      c = '/';
    }
  }
  return path;
}
#else
const std::string& NormalizedPath(const std::string& p)
{
  return p;
}
#endif // _WIN32


Snapshot::Snapshot(Snapshot&&) = default;
Snapshot::~Snapshot() = default;

Snapshot::Snapshot(Database db) : 
  m_database(std::make_unique<Database>(std::move(db)))
{
  if (!m_database->good())
    throw std::runtime_error("snapshot constructor expects a good() database");
}

/**
 * \brief returns the database associated with the snapshot
 */
Database& Snapshot::database() const
{
  return *m_database;
}

/**
 * \brief opens a snapshot
 * \param p  the path of the snapshot
 */
Snapshot Snapshot::open(const std::filesystem::path& p)
{
  Database db;
  db.open(p);
  return Snapshot(std::move(db));
}

static void insert_enum_values(Database& db)
{
  sql::Statement stmt{ db };

  stmt.prepare("INSERT INTO symbolKind (id, name) VALUES(?, ?)");

  enumerateSymbolKind([&stmt](SymbolKind k) {
    stmt.bind(1, static_cast<int>(k));
    stmt.bind(2, getSymbolKindString(k));
    stmt.step();
    stmt.reset();
    });

  stmt.finalize();

  {
    stmt.prepare("INSERT INTO accessSpecifier (value, name) VALUES(?, ?)");

    enumerateAccessSpecifier([&stmt](AccessSpecifier access) {
      stmt.bind(1, static_cast<int>(access));
      stmt.bind(2, getAccessSpecifierString(access));
      stmt.step();
      stmt.reset();
      });

    stmt.finalize();
  }

  {
    stmt.prepare("INSERT INTO diagnosticLevel (value, name) VALUES(?, ?)");

    enumerateDiagnosticLevel([&stmt](DiagnosticLevel lvl) {
      stmt.bind(1, static_cast<int>(lvl));
      stmt.bind(2, getDiagnosticLevelString(lvl));
      stmt.step();
      stmt.reset();
      });

    stmt.finalize();
  }

  /*stmt.prepare("INSERT INTO relationPredicate (id, name) VALUES(?, ?)");

  enumerateRelationKind([&stmt](RelationKind k) {
    stmt.bind(1, static_cast<int>(k));
    stmt.bind(2, getRelationKindString(k));
    stmt.step();
    stmt.reset();
    });

  stmt.finalize();*/

  {
    stmt.prepare("INSERT INTO symbolFlag (name, value) VALUES(?, ?)");

    enumerateSymbolFlag([&stmt](Symbol::Flag f) {
      stmt.bind(1, getSymbolFlagString(f));
      stmt.bind(2, static_cast<int>(f));
      stmt.step();
      stmt.reset();
      });

    stmt.finalize();
  }

  {
    stmt.prepare("INSERT INTO symbolReferenceFlag (name, value) VALUES(?, ?)");

    enumerateSymbolReferenceFlag([&stmt](SymbolReference::Flag f) {
      stmt.bind(1, getSymbolReferenceFlagString(f));
      stmt.bind(2, static_cast<int>(f));
      stmt.step();
      stmt.reset();
      });

    stmt.finalize();
  }
}

/**
 * \brief creates a new empty snapshot
 * \param p  file path
 */
Snapshot Snapshot::create(const std::filesystem::path& p)
{
  Database db;
  db.create(p);
  
  if (!sql::exec(db, snapshot::db_init_statements()))
    throw std::runtime_error("failed to create snapshot database");

  sql::runTransacted(db, [&db]() {
    insert_enum_values(db);
    });

  return Snapshot(std::move(db));
}

std::string Snapshot::normalizedPath(std::string p)
{
  return cppscanner::NormalizedPath(std::move(p));
}

void Snapshot::setProperty(const std::string& key, const std::string& value)
{
  sql::Statement stmt{ database() };

  stmt.prepare("INSERT OR REPLACE INTO info (key, value) VALUES (?,?)");

  stmt.bind(1, key.c_str());
  stmt.bind(2, value.c_str());

  stmt.step();

  stmt.finalize();
}

void Snapshot::setProperty(const std::string& key, bool value)
{
  setProperty(key, value ? "true" : "false");
}

void Snapshot::setProperty(const std::string& key, const char* value)
{
  setProperty(key, std::string(value));
}

void Snapshot::setProperty(const std::string& key, const Path& path)
{
  return setProperty(key, path.str());
}

void Snapshot::insertFilePaths(const std::vector<File>& files)
{
  sql::Statement stmt{ database(), "INSERT OR IGNORE INTO file(id, path) VALUES(?,?)"};

  for (const File& f : files) {
    const std::string& fpath = NormalizedPath(f.path);
    stmt.bind(1, (int)f.id);
    stmt.bind(2, fpath.c_str());
    stmt.step();
    stmt.reset();
  }

  stmt.finalize();
}

void Snapshot::insertIncludes(const std::vector<Include>& includes)
{
  // We use INSERT OR IGNORE here so that duplicates are automatically ignored by sqlite.
  // See the UNIQUE constraint in the CREATE statement of the "include" table.

  sql::Statement stmt{ 
    database(), 
    "INSERT OR IGNORE INTO include (file_id, line, included_file_id) VALUES(?,?,?)"
  };

  for (const Include& inc : includes)
  {
    stmt.bind(1, (int)inc.fileID);
    stmt.bind(2, inc.line);
    stmt.bind(3, (int)inc.includedFileID);

    stmt.step();
    stmt.reset();
  }

  stmt.finalize();
}

class SymbolExtraInfoInserter
{
private:
  sql::Statement m_stmt;
  constexpr static int ParameterIndex = 1;
  constexpr static int Type = 2;
  constexpr static int Value = 3;

public:
  explicit SymbolExtraInfoInserter(Database& db) : m_stmt(db)
  {
    m_stmt.prepare("UPDATE symbol SET parameterIndex = ?, type = ?, value = ? WHERE id = ?");
  }

  void process(const std::vector<const IndexerSymbol*>& symbols)
  {
    for (auto sptr : symbols)
    {
      const IndexerSymbol& sym = *sptr;
      m_stmt.bind(4, sym.id.rawID());
      std::visit(*this, sym.extraInfo);
    }
  }

  void operator()(std::monostate)
  {

  }

  void operator()(const MacroInfo& info)
  {
    m_stmt.bind(ParameterIndex, nullptr);
    m_stmt.bind(Type, nullptr);
    m_stmt.bind(Value, info.definition);

    m_stmt.step();
    m_stmt.reset();
  }

  void operator()(const FunctionInfo& info)
  {
    m_stmt.bind(ParameterIndex, nullptr);
    m_stmt.bind(Type, info.returnType);
    m_stmt.bind(Value, nullptr);

    m_stmt.step();
    m_stmt.reset();
  }

  void operator()(const ParameterInfo& info)
  {
    m_stmt.bind(ParameterIndex, info.parameterIndex);
    m_stmt.bind(Type, info.type);

    if (!info.defaultValue.empty())
      m_stmt.bind(Value, info.defaultValue);
    else
      m_stmt.bind(Value, nullptr);

    m_stmt.step();
    m_stmt.reset();
  }

  void operator()(const EnumInfo& info)
  {
    m_stmt.bind(ParameterIndex, nullptr);
    m_stmt.bind(Value, nullptr);

    if (!info.underlyingType.empty())
      m_stmt.bind(Type, info.underlyingType);
    else
      m_stmt.bind(Type, nullptr);

    m_stmt.step();
    m_stmt.reset();
  }

  void operator()(const EnumConstantInfo& info)
  {
    m_stmt.bind(ParameterIndex, nullptr);
    m_stmt.bind(Type, nullptr);

    if (!info.expression.empty())
      m_stmt.bind(Value, info.expression);
    else
      m_stmt.bind(Value, nullptr);

    m_stmt.step();
    m_stmt.reset();
  }

  void operator()(const VariableInfo& info)
  {
    m_stmt.bind(ParameterIndex, nullptr);

    if (!info.type.empty())
      m_stmt.bind(Type, info.type);
    else
      m_stmt.bind(Type, nullptr);

    if (!info.init.empty())
      m_stmt.bind(Value, info.init);
    else
      m_stmt.bind(Value, nullptr);

    m_stmt.step();
    m_stmt.reset();
  }

  void operator()(const NamespaceAliasInfo& info)
  {
    m_stmt.bind(ParameterIndex, nullptr);
    m_stmt.bind(Type, nullptr);

    if (!info.value.empty())
      m_stmt.bind(Value, info.value);
    else
      m_stmt.bind(Value, nullptr);


    m_stmt.step();
    m_stmt.reset();
  }
};

void insert_symbols_extra_info(Database& db, const std::vector<const IndexerSymbol*>& symbols)
{
  SymbolExtraInfoInserter inserter{ db };
  inserter.process(symbols);
}

void Snapshot::insertSymbols(const std::vector<const IndexerSymbol*>& symbols)
{
  if (symbols.empty()) {
    return;
  }

  sql::Statement stmt{ 
    database(),
    "INSERT OR REPLACE INTO symbol(id, kind, parent, name, flags) VALUES(?,?,?,?,?)" };

  for (auto sptr : symbols)
  {
    const IndexerSymbol& sym = *sptr;

    stmt.bind(1, sym.id.rawID());
    stmt.bind(2, static_cast<int>(sym.kind));

    if (sym.parentId.isValid())
      stmt.bind(3, sym.parentId.rawID());
    else
      stmt.bind(3, nullptr);

    stmt.bind(4, sym.name.c_str());

    stmt.bind(5, sym.flags);

    stmt.step();
    stmt.reset();
  }

  stmt.finalize();

  insert_symbols_extra_info(database(), symbols);
}

void Snapshot::insertBaseOfs(const std::vector<BaseOf>& bofs)
{
  if (bofs.empty()) {
    return;
  }

  sql::Statement stmt{ 
    database(),
    "INSERT OR IGNORE INTO baseOf(baseClassID, derivedClassID, access) VALUES(?,?,?)" 
  };

  for (const BaseOf& bof : bofs)
  {
    stmt.bind(1, bof.baseClassID.rawID());
    stmt.bind(2, bof.derivedClassID.rawID());
    stmt.bind(3, static_cast<int>(bof.accessSpecifier));

    stmt.step();
    stmt.reset();
  }
}

void Snapshot::insertOverrides(const std::vector<Override>& overrides)
{
  if (overrides.empty()) {
    return;
  }

  sql::Statement stmt{ 
    database(),
    "INSERT OR IGNORE INTO override(overrideMethodID, baseMethodID) VALUES(?,?)" 
  };

  for (const Override& ov : overrides)
  {
    stmt.bind(1, ov.overrideMethodID.rawID());
    stmt.bind(2, ov.baseMethodID.rawID());

    stmt.step();
    stmt.reset();
  }
}

void Snapshot::insertDiagnostics(const std::vector<Diagnostic>& diagnostics)
{
  if (diagnostics.empty()) {
    return;
  }

  sql::Statement stmt{ 
    database(),
    "INSERT OR IGNORE INTO diagnostic(level, fileID, line, column, message) VALUES(?,?,?,?,?)" 
  };

  for (const Diagnostic& d : diagnostics)
  {
    stmt.bind(1, static_cast<int>(d.level));
    stmt.bind(2, static_cast<int>(d.fileID));
    stmt.bind(3, d.position.line());
    stmt.bind(4, d.position.column());
    stmt.bind(5, d.message);

    stmt.step();
    stmt.reset();
  }
}

template<typename T, typename F>
T readUniqueRow(sql::Statement& stmt, F&& func)
{
  if (!stmt.step())
  {
    throw std::runtime_error("no rows");
  }

  T value{ func(stmt) };

  if (stmt.step())
  {
    throw std::runtime_error("no unique row");
  }

  return value;
}


template<typename T, typename F>
std::vector<T> readRowsAsVector(sql::Statement& stmt, F&& func)
{
  std::vector<T> r;

  while (stmt.step())
  {
    r.push_back(func(stmt));
  }

  return r;
}

std::vector<Include> Snapshot::loadAllIncludesInFile(FileID fid)
{
  sql::Statement stmt{ 
    database(),
    "SELECT line, included_file_id FROM include WHERE file_id = ?"
  };

  stmt.bind(1, (int)fid);

  auto read_row = [fid](sql::Statement& q) -> Include {
    Include row;
    row.fileID = fid;
    row.line = q.columnInt(0);
    row.includedFileID = q.columnInt(1);
    return row;
    };

  return readRowsAsVector<Include>(stmt, read_row);
}

void Snapshot::removeAllIncludesInFile(FileID fid)
{
  sql::Statement stmt{ 
    database(),
    "DELETE FROM include WHERE file_id = ?"
  };

  stmt.bind(1, (int)fid);

  stmt.step();
}

std::vector<SymbolReference> Snapshot::loadSymbolReferencesInFile(FileID fid)
{
  sql::Statement stmt{ 
    database(),
    "SELECT symbol_id, line, col, parent_symbol_id, flags FROM symbolReference WHERE file_id = ?"
  };

  stmt.bind(1, (int)fid);

  auto read_row = [fid](sql::Statement& q) -> SymbolReference {
    SymbolReference r;
    r.fileID = fid;
    r.symbolID = SymbolID::fromRawID(q.columnInt64(0));
    r.position = FilePosition(q.columnInt(1), q.columnInt(2));
    r.referencedBySymbolID = SymbolID::fromRawID(q.columnInt(3));
    r.flags = q.columnInt(4);
    return r;
  };

  return readRowsAsVector<SymbolReference>(stmt, read_row);
}

void Snapshot::removeAllSymbolReferencesInFile(FileID fid)
{
  sql::Statement stmt{ 
    database(),
    "DELETE FROM symbolReference WHERE file_id = ?"
  };

  stmt.bind(1, (int)fid);

  stmt.step();
}

std::vector<Diagnostic> Snapshot::loadDiagnosticsInFile(FileID fid)
{
  sql::Statement stmt{ 
    database(),
    "SELECT level, line, column, message FROM diagnostic WHERE fileID = ?"
  };

  stmt.bind(1, (int)fid);

  auto read_row = [fid](sql::Statement& q) -> Diagnostic {
    Diagnostic d;
    d.fileID = fid;
    d.level = static_cast<DiagnosticLevel>(q.columnInt(0));
    d.position = FilePosition(q.columnInt(1), q.columnInt(2));
    d.message = q.column(3);
    return d;
    };

  return readRowsAsVector<Diagnostic>(stmt, read_row);
}

void Snapshot::removeAllDiagnosticsInFile(FileID fid)
{
  sql::Statement stmt{ 
    database(),
    "DELETE FROM diagnostic WHERE fileID = ?"
  };

  stmt.bind(1, (int)fid);

  stmt.step();
}

File readFileIdAndPath(sql::Statement& row)
{
  File f;
  f.id = row.columnInt(0);
  f.path = row.column(1);
  return f;
}

std::vector<File> Snapshot::getFiles() const
{
  sql::Statement stmt{ 
    database(),
    "SELECT id, path FROM file"
  };

  return readRowsAsVector<File>(stmt, readFileIdAndPath);
}

Include readInclude(sql::Statement& row)
{
  Include i;
  i.fileID = row.columnInt(0);
  i.includedFileID = row.columnInt(1);
  i.line = row.columnInt(2);
  return i;
}


std::vector<Include> Snapshot::getIncludedFiles(FileID fid) const
{
  sql::Statement stmt { 
    database(),
    "SELECT file_id, included_file_id, line FROM include WHERE file_id = ?"
  };

  stmt.bind(1, (int)fid);

  return readRowsAsVector<Include>(stmt, readInclude);
}

SymbolRecord readSymbol(sql::Statement& row)
{
  SymbolRecord s;
  s.id = SymbolID::fromRawID(row.columnInt64(0));
  s.kind = static_cast<SymbolKind>(row.columnInt(1));
  s.parentId = SymbolID::fromRawID(row.columnInt64(2));
  s.name = row.column(3);
  s.flags = row.columnInt(4);
  return s;
}

std::vector<SymbolRecord> Snapshot::findSymbolsByName(const std::string& name) const
{
  sql::Statement stmt{ 
    database(),
    "SELECT id, kind, parent, name, flags FROM symbol WHERE name = ?"
  };

  stmt.bind(1, name);
  return readRowsAsVector<SymbolRecord>(stmt, readSymbol);
}

std::vector<SymbolRecord> Snapshot::findSymbolsByName(const sql::Like& name) const
{
  sql::Statement stmt{ 
    database(),
    "SELECT id, kind, parent, name, flags FROM symbol WHERE name LIKE ?"
  };

  stmt.bind(1, name.str());
  return readRowsAsVector<SymbolRecord>(stmt, readSymbol);
}

SymbolRecord Snapshot::getSymbolByName(const std::string& name, SymbolID parentID) const
{
  std::vector<SymbolRecord> symbols;

  if (parentID == SymbolID()) {
    symbols = findSymbolsByName(name);
  } else {
    sql::Statement stmt{ 
      database(),
      "SELECT id, kind, parent, name, flags FROM symbol WHERE name = ? and parent = ?"
    };

    stmt.bind(1, name);
    stmt.bind(2, parentID.rawID());
    symbols = readRowsAsVector<SymbolRecord>(stmt, readSymbol);
  }

  if (symbols.size() != 1) {
    throw std::runtime_error("could not find unique symbol with given name: " + name);
  }

  return symbols.front();
}

SymbolRecord Snapshot::getSymbolByName(const sql::Like& name, SymbolID parentID) const
{
  std::vector<SymbolRecord> symbols;

  if (parentID == SymbolID()) {
    symbols = findSymbolsByName(name);
  } else {
    sql::Statement stmt{ 
      database(),
      "SELECT id, kind, parent, name, flags FROM symbol WHERE name LIKE ? and parent = ?"
    };

    stmt.bind(1, name.str());
    stmt.bind(2, parentID.rawID());
    symbols = readRowsAsVector<SymbolRecord>(stmt, readSymbol);
  }

  if (symbols.size() != 1) {
    throw std::runtime_error("could not find unique symbol with name like: " + name.str());
  }

  return symbols.front();
}

SymbolRecord Snapshot::getSymbol(const std::vector<std::string>& qualifiedName) const
{
  SymbolRecord s;

  for (const std::string& name : qualifiedName)
  {
    s = getSymbolByName(name, s.id);
  }

  return s;
}

std::vector<SymbolRecord> Snapshot::getSymbols(SymbolID parentID) const
{
  sql::Statement stmt{ 
    database(),
    "SELECT id, kind, parent, name, flags FROM symbol WHERE parent = ?"
  };

  stmt.bind(1, parentID.rawID());
  return readRowsAsVector<SymbolRecord>(stmt, readSymbol);
}

std::vector<SymbolRecord> Snapshot::getSymbols(SymbolID parentID, SymbolKind kind) const
{
  sql::Statement stmt{ 
    database(),
    "SELECT id, kind, parent, name, flags FROM symbol WHERE parent = ? AND kind = ?"
  };

  stmt.bind(1, parentID.rawID());
  stmt.bind(2, static_cast<int>(kind));
  return readRowsAsVector<SymbolRecord>(stmt, readSymbol);
}

inline static MacroRecord readMacroRecord(sql::Statement& row)
{
  MacroRecord s;
  s.id = SymbolID::fromRawID(row.columnInt64(0));
  s.kind = static_cast<SymbolKind>(row.columnInt(1));
  s.parentId = SymbolID::fromRawID(row.columnInt64(2));
  s.name = row.column(3);
  s.flags = row.columnInt(4);
  s.definition = row.column(5);
  return s;
}

MacroRecord Snapshot::getMacroRecord(const std::string& name) const
{
  static_assert((int)SymbolKind::Macro == 4, "Macro != 4");

  sql::Statement stmt{ 
    database(),
    "SELECT id, kind, parent, name, flags, value FROM symbol WHERE kind = 4 and name = ?"
  };

  stmt.bind(1, name);

  return readUniqueRow<MacroRecord>(stmt, readMacroRecord);
}

std::vector<MacroRecord> Snapshot::getMacroRecords(const std::string& name) const
{
  static_assert((int)SymbolKind::Macro == 4, "Macro != 4");

  sql::Statement stmt{ 
    database(),
    "SELECT id, kind, parent, name, flags, value FROM symbol WHERE kind = 4 and name = ?"
  };

  stmt.bind(1, name);

  return readRowsAsVector<MacroRecord>(stmt, readMacroRecord);
}

inline static NamespaceAliasRecord readNamespaceAliasRecord(sql::Statement& row)
{
  NamespaceAliasRecord r;
  r.id = SymbolID::fromRawID(row.columnInt64(0));
  r.kind = static_cast<SymbolKind>(row.columnInt(1));
  r.parentId = SymbolID::fromRawID(row.columnInt64(2));
  r.name = row.column(3);
  r.flags = row.columnInt(4);
  r.value = row.column(5);
  return r;
}

NamespaceAliasRecord Snapshot::getNamespaceAliasRecord(const std::string& name) const
{
  static_assert((int)SymbolKind::NamespaceAlias == 3);

  sql::Statement stmt{ 
    database(),
    "SELECT id, kind, parent, name, flags, value FROM symbol WHERE kind = 3 and name = ?"
  };

  stmt.bind(1, name);

  return readUniqueRow<NamespaceAliasRecord>(stmt, readNamespaceAliasRecord);
}

inline static EnumConstantRecord readEnumConstantRecord(sql::Statement& row)
{
  EnumConstantRecord s;
  s.id = SymbolID::fromRawID(row.columnInt64(0));
  s.kind = static_cast<SymbolKind>(row.columnInt(1));
  s.parentId = SymbolID::fromRawID(row.columnInt64(2));
  s.name = row.column(3);
  s.flags = row.columnInt(4);
  s.expression = row.column(5);
  return s;
}

EnumConstantRecord Snapshot::getEnumConstantRecord(SymbolID enumID, const std::string& name) const
{
  static_assert((int)SymbolKind::EnumConstant == 14);

  sql::Statement stmt{ 
    database(),
    "SELECT id, kind, parent, name, flags, value FROM symbol WHERE parent = ? AND kind = 14 AND name = ?"
  };

  stmt.bind(1, enumID.rawID());
  stmt.bind(2, name);

  return readUniqueRow<EnumConstantRecord>(stmt, readEnumConstantRecord);
}

std::vector<EnumConstantRecord> Snapshot::getEnumConstantsForEnum(SymbolID enumID) const
{
  static_assert((int)SymbolKind::EnumConstant == 14, "EnumConstant != 14");

  sql::Statement stmt{ 
    database(),
    "SELECT id, kind, parent, name, flags, value FROM symbol WHERE parent = ? AND kind = 14"
  };

  stmt.bind(1, enumID.rawID());
  return readRowsAsVector<EnumConstantRecord>(stmt, readEnumConstantRecord);
}

inline static ParameterRecord readParameterRecord(sql::Statement& row)
{
  ParameterRecord s;
  s.id = SymbolID::fromRawID(row.columnInt64(0));
  s.kind = static_cast<SymbolKind>(row.columnInt(1));
  s.parentId = SymbolID::fromRawID(row.columnInt64(2));
  s.name = row.column(3);
  s.flags = row.columnInt(4);
  s.parameterIndex = row.columnInt(5);
  s.type = row.column(6);
  s.defaultValue = row.column(7);
  return s;
}

std::vector<ParameterRecord> Snapshot::getParameters(SymbolID symbolId, SymbolKind parameterKind) const
{
  sql::Statement stmt{ 
    database(),
    "SELECT id, kind, parent, name, flags, parameterIndex, type, value FROM symbol WHERE parent = ? AND kind = ?"
  };

  stmt.bind(1, symbolId.rawID());
  stmt.bind(2, static_cast<int>(parameterKind));
  return readRowsAsVector<ParameterRecord>(stmt, readParameterRecord);
}

std::vector<ParameterRecord> Snapshot::getFunctionParameters(SymbolID functionId, SymbolKind kind) const
{
  return getParameters(functionId, kind);
}

inline static VariableRecord readVariableRecord(sql::Statement& row)
{
  VariableRecord r;
  r.id = SymbolID::fromRawID(row.columnInt64(0));
  r.kind = static_cast<SymbolKind>(row.columnInt(1));
  r.parentId = SymbolID::fromRawID(row.columnInt64(2));
  r.name = row.column(3);
  r.flags = row.columnInt(4);
  r.type = row.column(5);
  r.init = row.column(6);
  return r;
}

inline static std::vector<VariableRecord> readVariableLikeRecords(Database& db, SymbolID parentId, SymbolKind kind)
{
  sql::Statement stmt{ 
    db,
    "SELECT id, kind, parent, name, flags, type, value FROM symbol WHERE parent = ? AND kind = ?"
  };

  stmt.bind(1, parentId.rawID());
  stmt.bind(2, static_cast<int>(kind));
  return readRowsAsVector<VariableRecord>(stmt, readVariableRecord);
}

VariableRecord Snapshot::getField(SymbolID classId, const std::string& name) const
{
  static_assert((int)SymbolKind::Field == 13);

  sql::Statement stmt{ 
    database(),
    "SELECT id, kind, parent, name, flags, type, value FROM symbol WHERE parent = ? AND kind = 13 AND name = ?"
  };

  stmt.bind(1, classId.rawID());
  stmt.bind(2, name);
  return readUniqueRow<VariableRecord>(stmt, readVariableRecord);
}

std::vector<VariableRecord> Snapshot::getFields(SymbolID classId) const
{
  return readVariableLikeRecords(database(), classId, SymbolKind::Field);
}

std::vector<VariableRecord> Snapshot::getStaticProperties(SymbolID classId) const
{
  return readVariableLikeRecords(database(), classId, SymbolKind::StaticProperty);
}

SymbolID sqlColumnAsSymbolID(sql::Statement& row, int col)
{
  return SymbolID::fromRawID(row.columnInt64(col));
}

AccessSpecifier sqlColumnAsAccessSpecifier(sql::Statement& row, int col)
{
  return static_cast<AccessSpecifier>(row.columnInt(col));
}

std::vector<BaseOf> Snapshot::getBasesOf(SymbolID classID) const
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

  return readRowsAsVector<BaseOf>(stmt, rr);
}

std::vector<Override> Snapshot::getOverridesOf(SymbolID methodID) const
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

  return readRowsAsVector<Override>(stmt, rr);
}

SymbolReference readSymbolReference(sql::Statement& row)
{
  SymbolReference r;
  r.symbolID = SymbolID::fromRawID(row.columnInt64(0));
  r.fileID = row.columnInt(1);
  r.position = FilePosition(row.columnInt(2), row.columnInt(3));
  r.referencedBySymbolID = SymbolID::fromRawID(row.columnInt64(4));
  r.flags = row.columnInt(5);
  return r;
}

std::vector<SymbolReference> Snapshot::findReferences(SymbolID symbolID)
{
  sql::Statement stmt{ 
    database(),
    "SELECT symbol_id, file_id, line, col, parent_symbol_id, flags FROM symbolReference WHERE symbol_id = ?"
  };

  stmt.bind(1, symbolID.rawID());

  return readRowsAsVector<SymbolReference>(stmt, readSymbolReference);
}

namespace snapshot
{

const char* db_init_statements()
{
  return SQL_CREATE_STATEMENTS;
}

void insertFiles(Snapshot& snapshot, const std::vector<File>& files)
{
  sql::Statement stmt{ snapshot.database(), "INSERT OR REPLACE INTO file(id, path, content) VALUES(?,?,?)"};

  for (const File& f : files) 
  {
    const std::string& fpath = NormalizedPath(f.path);
    stmt.bind(1, (int)f.id);
    stmt.bind(2, fpath.c_str());
    stmt.bind(3, f.content.c_str());
    stmt.step();
    stmt.reset();
  }

  stmt.finalize();
}

void insertSymbolReferences(Snapshot& snapshot, const std::vector<SymbolReference>& references)
{
  sql::Statement stmt{
    snapshot.database(),
    "INSERT INTO symbolReference (symbol_id, file_id, line, col, parent_symbol_id, flags) VALUES (?,?,?,?,?,?)"
  };

  for (const SymbolReference& ref : references)
  {
    stmt.bind(1, ref.symbolID.rawID());
    stmt.bind(2, (int)ref.fileID);
    stmt.bind(3, ref.position.line());
    stmt.bind(4, ref.position.column());

    if (ref.referencedBySymbolID.isValid())
      stmt.bind(5, ref.referencedBySymbolID.rawID());
    else 
      stmt.bind(5, nullptr);

    stmt.bind(6, ref.flags);

    stmt.step();
    stmt.reset();
  }

  stmt.finalize();
}

} // namespace snapshot

} // namespace cppscanner

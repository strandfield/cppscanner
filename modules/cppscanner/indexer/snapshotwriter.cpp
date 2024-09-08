// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#include "snapshotwriter.h"

#include "indexersymbol.h"
#include "symbolrecorditerator.h"

#include "cppscanner/database/readrows.h"
#include "cppscanner/database/transaction.h"

#include "cppscanner/index/symbol.h"

#include <algorithm>

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

CREATE TABLE "symbol" (
  "id"                INTEGER NOT NULL PRIMARY KEY UNIQUE,
  "kind"              INTEGER NOT NULL,
  "parent"            INTEGER,
  "name"              TEXT NOT NULL,
  "flags"             INTEGER NOT NULL DEFAULT 0,
  "parameterIndex"    INTEGER,
  "type"              TEXT,
  "value"             TEXT,
  isLocal             INT GENERATED ALWAYS AS ((flags & 1) = 1) VIRTUAL,
  isProtected         INT GENERATED ALWAYS AS ((flags & 2) = 2) VIRTUAL,
  isPrivate           INT GENERATED ALWAYS AS ((flags & 4) = 4) VIRTUAL,
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

static void insert_enum_values(Database& db)
{
  sql::Statement stmt{ db };

  stmt.prepare("INSERT INTO symbolKind (id, name) VALUES(?, ?)");

  enumerateSymbolKind([&stmt](SymbolKind k) {
    stmt.bind(1, static_cast<int>(k));
    stmt.bind(2, getSymbolKindString(k));
    stmt.insert();
    });

  stmt.finalize();

  {
    stmt.prepare("INSERT INTO accessSpecifier (value, name) VALUES(?, ?)");

    enumerateAccessSpecifier([&stmt](AccessSpecifier access) {
      stmt.bind(1, static_cast<int>(access));
      stmt.bind(2, getAccessSpecifierString(access));
      stmt.insert();
      });

    stmt.finalize();
  }

  {
    stmt.prepare("INSERT INTO diagnosticLevel (value, name) VALUES(?, ?)");

    enumerateDiagnosticLevel([&stmt](DiagnosticLevel lvl) {
      stmt.bind(1, static_cast<int>(lvl));
      stmt.bind(2, getDiagnosticLevelString(lvl));
      stmt.insert();
      });

    stmt.finalize();
  }

  {
    stmt.prepare("INSERT INTO symbolReferenceFlag (name, value) VALUES(?, ?)");

    enumerateSymbolReferenceFlag([&stmt](SymbolReference::Flag f) {
      stmt.bind(1, getSymbolReferenceFlagString(f));
      stmt.bind(2, static_cast<int>(f));
      stmt.insert();
      });

    stmt.finalize();
  }
}

SnapshotWriter::SnapshotWriter(SnapshotWriter&&) = default;
SnapshotWriter::~SnapshotWriter() = default;

SnapshotWriter::SnapshotWriter(const std::filesystem::path& databasePath) :
  m_database(std::make_unique<Database>())

{
  database().create(databasePath);

  if (!sql::exec(database(), snapshot::db_init_statements()))
    throw std::runtime_error("failed to create snapshot database");

  sql::runTransacted(database(), [this]() {
    insert_enum_values(database());
    });
}

/**
 * \brief returns the database associated with the snapshot
 */
Database& SnapshotWriter::database() const
{
  return *m_database;
}

std::string SnapshotWriter::normalizedPath(std::string p)
{
  return cppscanner::NormalizedPath(std::move(p));
}

void SnapshotWriter::setProperty(const std::string& key, const std::string& value)
{
  sql::Statement stmt{ database() };

  stmt.prepare("INSERT OR REPLACE INTO info (key, value) VALUES (?,?)");

  stmt.bind(1, key.c_str());
  stmt.bind(2, value.c_str());

  stmt.step();

  stmt.finalize();
}

void SnapshotWriter::setProperty(const std::string& key, bool value)
{
  setProperty(key, value ? "true" : "false");
}

void SnapshotWriter::setProperty(const std::string& key, const char* value)
{
  setProperty(key, std::string(value));
}

void SnapshotWriter::setProperty(const std::string& key, const Path& path)
{
  return setProperty(key, path.str());
}

void SnapshotWriter::insertFilePaths(const std::vector<File>& files)
{
  sql::Statement stmt{ database(), "INSERT OR IGNORE INTO file(id, path) VALUES(?,?)"};

  for (const File& f : files) {
    const std::string& fpath = NormalizedPath(f.path);
    stmt.bind(1, (int)f.id);
    stmt.bind(2, fpath.c_str());
    stmt.insert();
  }

  stmt.finalize();
}

void SnapshotWriter::insertIncludes(const std::vector<Include>& includes)
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

    stmt.insert();
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
    m_stmt.bind(Value, std::string_view(info.definition));

    m_stmt.insert();
  }

  void operator()(const FunctionInfo& info)
  {
    m_stmt.bind(ParameterIndex, nullptr);
    m_stmt.bind(Type, std::string_view(info.returnType));
    m_stmt.bind(Value, nullptr);

    m_stmt.insert();
  }

  void operator()(const ParameterInfo& info)
  {
    m_stmt.bind(ParameterIndex, info.parameterIndex);
    m_stmt.bind(Type, std::string_view(info.type));

    if (!info.defaultValue.empty())
      m_stmt.bind(Value, std::string_view(info.defaultValue));
    else
      m_stmt.bind(Value, nullptr);

    m_stmt.insert();
  }

  void operator()(const EnumInfo& info)
  {
    m_stmt.bind(ParameterIndex, nullptr);
    m_stmt.bind(Value, nullptr);

    if (!info.underlyingType.empty())
      m_stmt.bind(Type, std::string_view(info.underlyingType));
    else
      m_stmt.bind(Type, nullptr);

    m_stmt.insert();
  }

  void operator()(const EnumConstantInfo& info)
  {
    m_stmt.bind(ParameterIndex, nullptr);
    m_stmt.bind(Type, nullptr);

    if (!info.expression.empty())
      m_stmt.bind(Value, std::string_view(info.expression));
    else
      m_stmt.bind(Value, nullptr);

    m_stmt.insert();
  }

  void operator()(const VariableInfo& info)
  {
    m_stmt.bind(ParameterIndex, nullptr);

    if (!info.type.empty())
      m_stmt.bind(Type, std::string_view(info.type));
    else
      m_stmt.bind(Type, nullptr);

    if (!info.init.empty())
      m_stmt.bind(Value, std::string_view(info.init));
    else
      m_stmt.bind(Value, nullptr);

    m_stmt.insert();
  }

  void operator()(const NamespaceAliasInfo& info)
  {
    m_stmt.bind(ParameterIndex, nullptr);
    m_stmt.bind(Type, nullptr);

    if (!info.value.empty())
      m_stmt.bind(Value, std::string_view(info.value));
    else
      m_stmt.bind(Value, nullptr);

    m_stmt.insert();
  }
};

void insert_symbols_extra_info(Database& db, const std::vector<const IndexerSymbol*>& symbols)
{
  SymbolExtraInfoInserter inserter{ db };
  inserter.process(symbols);
}

void SnapshotWriter::insertSymbols(const std::vector<const IndexerSymbol*>& symbols)
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

    stmt.insert();
  }

  stmt.finalize();

  insert_symbols_extra_info(database(), symbols);
}

void SnapshotWriter::insertBaseOfs(const std::vector<BaseOf>& bofs)
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

    stmt.insert();
  }
}

void SnapshotWriter::insertOverrides(const std::vector<Override>& overrides)
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

    stmt.insert();
  }
}

void SnapshotWriter::insertDiagnostics(const std::vector<Diagnostic>& diagnostics)
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
    stmt.bind(5, std::string_view(d.message));

    stmt.insert();
  }
}

std::vector<Include> SnapshotWriter::loadAllIncludesInFile(FileID fid)
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

  return sql::readRowsAsVector<Include>(stmt, read_row);
}

void SnapshotWriter::removeAllIncludesInFile(FileID fid)
{
  sql::Statement stmt{ 
    database(),
    "DELETE FROM include WHERE file_id = ?"
  };

  stmt.bind(1, (int)fid);

  stmt.step();
}

std::vector<SymbolReference> SnapshotWriter::loadSymbolReferencesInFile(FileID fid)
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

  return sql::readRowsAsVector<SymbolReference>(stmt, read_row);
}

void SnapshotWriter::removeAllSymbolReferencesInFile(FileID fid)
{
  sql::Statement stmt{ 
    database(),
    "DELETE FROM symbolReference WHERE file_id = ?"
  };

  stmt.bind(1, (int)fid);

  stmt.step();
}

std::vector<Diagnostic> SnapshotWriter::loadDiagnosticsInFile(FileID fid)
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

  return sql::readRowsAsVector<Diagnostic>(stmt, read_row);
}

void SnapshotWriter::removeAllDiagnosticsInFile(FileID fid)
{
  sql::Statement stmt{ 
    database(),
    "DELETE FROM diagnostic WHERE fileID = ?"
  };

  stmt.bind(1, (int)fid);

  stmt.step();
}

namespace snapshot
{

const char* db_init_statements()
{
  return SQL_CREATE_STATEMENTS;
}

void insertFiles(SnapshotWriter& snapshot, const std::vector<File>& files)
{
  sql::Statement stmt{ snapshot.database(), "INSERT OR REPLACE INTO file(id, path, content) VALUES(?,?,?)"};

  for (const File& f : files) 
  {
    const std::string& fpath = NormalizedPath(f.path);
    stmt.bind(1, (int)f.id);
    stmt.bind(2, fpath.c_str());
    stmt.bind(3, f.content.c_str());
    stmt.insert();
  }

  stmt.finalize();
}

void insertSymbolReferences(SnapshotWriter& snapshot, const std::vector<SymbolReference>& references)
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

    stmt.insert();
  }

  stmt.finalize();
}

} // namespace snapshot

} // namespace cppscanner

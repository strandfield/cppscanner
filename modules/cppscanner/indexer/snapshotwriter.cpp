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

static_assert(SymbolFlag::Local == 1);
static_assert(SymbolFlag::FromProject == 2);
static_assert(SymbolFlag::Protected == 4);
static_assert(SymbolFlag::Private == 8);

static_assert(MacroInfo::MacroUsedAsHeaderGuard == 32);
static_assert(MacroInfo::FunctionLike == 64);

static_assert(static_cast<int>(SymbolKind::Macro) == 5);
static_assert(static_cast<int>(SymbolKind::NamespaceAlias) == 4);
static_assert(static_cast<int>(SymbolKind::Enum) == 6);
static_assert(static_cast<int>(SymbolKind::EnumClass) == 7);
static_assert(static_cast<int>(SymbolKind::EnumConstant) == 14);

static_assert(FunctionInfo::Inline == 32);
static_assert(FunctionInfo::Static == 64);
static_assert(FunctionInfo::Constexpr == 128);
static_assert(FunctionInfo::Consteval == 256);
static_assert(FunctionInfo::Noexcept == 512);
static_assert(FunctionInfo::Default == 1024);
static_assert(FunctionInfo::Delete == 2048);
static_assert(FunctionInfo::Const == 4096);
static_assert(FunctionInfo::Virtual == 8192);
static_assert(FunctionInfo::Pure == 16384);
static_assert(FunctionInfo::Override == 32768);
static_assert(FunctionInfo::Final == 65536);
static_assert(FunctionInfo::Explicit == 131072);

static_assert(static_cast<int>(SymbolKind::Function) == 18);
static_assert(static_cast<int>(SymbolKind::Method) == 19);
static_assert(static_cast<int>(SymbolKind::StaticMethod) == 20);
static_assert(static_cast<int>(SymbolKind::Constructor) == 21);
static_assert(static_cast<int>(SymbolKind::Destructor) == 22);
static_assert(static_cast<int>(SymbolKind::Operator) == 23);
static_assert(static_cast<int>(SymbolKind::ConversionFunction) == 24);

static_assert(static_cast<int>(SymbolKind::Parameter) == 26);
static_assert(static_cast<int>(SymbolKind::TemplateTypeParameter) == 27);
static_assert(static_cast<int>(SymbolKind::TemplateTemplateParameter) == 28);
static_assert(static_cast<int>(SymbolKind::NonTypeTemplateParameter) == 29);

static_assert(VariableInfo::Const == 32);
static_assert(VariableInfo::Constexpr == 64);
static_assert(VariableInfo::Static == 128);
static_assert(VariableInfo::Mutable == 256);
static_assert(VariableInfo::ThreadLocal == 512);
static_assert(VariableInfo::Inline == 1024);

static_assert(static_cast<int>(SymbolKind::Variable) == 15);
static_assert(static_cast<int>(SymbolKind::Field) == 16);
static_assert(static_cast<int>(SymbolKind::StaticProperty) == 17);

static_assert(SymbolReference::Declaration == 1);
static_assert(SymbolReference::Definition == 2);
static_assert(SymbolReference::Read == 4);
static_assert(SymbolReference::Write == 8);
static_assert(SymbolReference::Call == 16);
static_assert(SymbolReference::Dynamic == 32);
static_assert(SymbolReference::Implicit == 128);

static_assert(FilePosition::ColumnBits == 12);

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
  isLocal             INT GENERATED ALWAYS AS ((flags & 1) = 1) VIRTUAL,
  isFromProject       INT GENERATED ALWAYS AS ((flags & 2) != 0) VIRTUAL,
  isProtected         INT GENERATED ALWAYS AS ((flags & 4) = 4) VIRTUAL,
  isPrivate           INT GENERATED ALWAYS AS ((flags & 8) = 8) VIRTUAL,
  FOREIGN KEY("kind") REFERENCES "symbolKind"("id")
);

CREATE TABLE macroInfo (
  id              INTEGER NOT NULL PRIMARY KEY UNIQUE,
  definition      TEXT,
  FOREIGN KEY(id) REFERENCES symbol(id)
);

CREATE VIEW macroRecord (id, name, flags, definition, isUsedAsHeaderGuard, isFunctionLike, kind, parent) AS
  SELECT symbol.id, symbol.name, symbol.flags, macroInfo.definition, ((flags & 32) = 32), ((flags & 64) = 64), 5, NULL
  FROM symbol
  LEFT JOIN macroInfo ON symbol.id = macroInfo.id
  WHERE symbol.kind = 5;

CREATE TABLE namespaceAliasInfo (
  id              INTEGER NOT NULL PRIMARY KEY UNIQUE,
  value           TEXT,
  FOREIGN KEY(id) REFERENCES symbol(id)
);

CREATE VIEW namespaceAliasRecord (id, name, parent, flags, value, kind) AS
  SELECT symbol.id, symbol.name, symbol.parent, symbol.flags, namespaceAliasInfo.value, 4
  FROM symbol
  LEFT JOIN namespaceAliasInfo ON symbol.id = namespaceAliasInfo.id
  WHERE symbol.kind = 4;

CREATE TABLE enumInfo (
  id              INTEGER NOT NULL PRIMARY KEY UNIQUE,
  integerType     TEXT,
  FOREIGN KEY(id) REFERENCES symbol(id)
);

CREATE VIEW enumRecord (id, parent, name, integerType, kind, flags) AS
  SELECT symbol.id, symbol.parent, symbol.name, enumInfo.integerType, symbol.kind, symbol.flags
  FROM symbol
  LEFT JOIN enumInfo ON symbol.id = enumInfo.id
  WHERE (symbol.kind = 6 OR symbol.kind = 7);

CREATE TABLE enumConstantInfo (
  id              INTEGER NOT NULL PRIMARY KEY UNIQUE,
  value           INTEGER,
  expression      TEXT,
  FOREIGN KEY(id) REFERENCES symbol(id)
);

CREATE VIEW enumConstantRecord (id, parent, name, value, expression, kind, flags) AS
  SELECT symbol.id, symbol.parent, symbol.name, enumConstantInfo.value, enumConstantInfo.expression, 14, symbol.flags
  FROM symbol
  LEFT JOIN enumConstantInfo ON symbol.id = enumConstantInfo.id
  WHERE symbol.kind = 14;

CREATE TABLE functionInfo (
  id              INTEGER NOT NULL PRIMARY KEY UNIQUE,
  returnType      TEXT,
  FOREIGN KEY(id) REFERENCES symbol(id)
);

CREATE VIEW functionRecord (
  id, parent, kind, name, returnType, flags, 
  isInline, isStatic, isConstexpr, isConsteval, 
  isNoexcept, isDefault, isDelete, isConst, 
  isVirtual, isPure, isOverride, isFinal, 
  isExplicit
  ) AS
  SELECT 
    symbol.id, symbol.parent, symbol.kind, symbol.name, functionInfo.returnType, symbol.flags,
    (symbol.flags & 32 != 0), (symbol.flags & 64 != 0), (symbol.flags & 128 != 0), (symbol.flags & 256 != 0), 
    (symbol.flags & 512 != 0), (symbol.flags & 1024 != 0), (symbol.flags & 2048 != 0), (symbol.flags & 4096 != 0),
    (symbol.flags & 8192 != 0), (symbol.flags & 16384 != 0), (symbol.flags & 32768 != 0), (symbol.flags & 65536 != 0),
    (symbol.flags & 131072 != 0)
  FROM symbol
  LEFT JOIN functionInfo ON symbol.id = functionInfo.id
  WHERE (symbol.kind >= 18 AND symbol.kind <= 24);

CREATE TABLE parameterInfo (
  id              INTEGER NOT NULL PRIMARY KEY UNIQUE,
  parameterIndex  INTEGER,
  type            TEXT,
  defaultValue    TEXT,
  FOREIGN KEY(id) REFERENCES symbol(id)
);

CREATE VIEW parameterRecord (id, parent, kind, parameterIndex, type, name, defaultValue, flags) AS
  SELECT symbol.id, symbol.parent, symbol.kind, parameterInfo.parameterIndex, parameterInfo.type, symbol.name, parameterInfo.defaultValue, symbol.flags
  FROM symbol
  LEFT JOIN parameterInfo ON symbol.id = parameterInfo.id
  WHERE (symbol.kind >= 26 AND symbol.kind <= 29);

CREATE TABLE variableInfo (
  id              INTEGER NOT NULL PRIMARY KEY UNIQUE,
  type            TEXT,
  init            TEXT,
  FOREIGN KEY(id) REFERENCES symbol(id)
);

CREATE VIEW variableRecord (
  id, parent, kind, type, name, init, flags,
  isConst, isConstexpr, isStatic, isMutable,
  isThreadLocal, isInline
  ) AS
  SELECT 
    symbol.id, symbol.parent, symbol.kind, variableInfo.type, symbol.name, variableInfo.init, symbol.flags,
    (symbol.flags & 32 != 0), (symbol.flags & 64 != 0), (symbol.flags & 128 != 0), (symbol.flags & 256 != 0), 
    (symbol.flags & 512 != 0), (symbol.flags & 1024 != 0)
  FROM symbol
  LEFT JOIN variableInfo ON symbol.id = variableInfo.id
  WHERE (symbol.kind = 15 OR symbol.kind = 16 OR symbol.kind = 17);

CREATE TABLE "symbolReference" (
  "symbol_id"                     INTEGER NOT NULL,
  "file_id"                       INTEGER NOT NULL,
  "line"                          INTEGER NOT NULL,
  "col"                           INTEGER NOT NULL,
  "parent_symbol_id"              INTEGER,
  "flags"                         INTEGER NOT NULL DEFAULT 0,
  isDeclaration                   INT GENERATED ALWAYS AS ((flags & 1) != 0) VIRTUAL,
  isDefinition                    INT GENERATED ALWAYS AS ((flags & 2) != 0) VIRTUAL,
  isReference                     INT GENERATED ALWAYS AS ((flags & 3) = 0) VIRTUAL,
  isRead                          INT GENERATED ALWAYS AS ((flags & 4) != 0) VIRTUAL,
  isWrite                         INT GENERATED ALWAYS AS ((flags & 8) != 0) VIRTUAL,
  isCall                          INT GENERATED ALWAYS AS ((flags & 16) != 0) VIRTUAL,
  isDynamic                       INT GENERATED ALWAYS AS ((flags & 32) != 0) VIRTUAL,
  isImplicit                      INT GENERATED ALWAYS AS ((flags & 128) != 0) VIRTUAL,
  FOREIGN KEY("symbol_id")        REFERENCES "symbol"("id"),
  FOREIGN KEY("file_id")          REFERENCES "file"("id"),
  FOREIGN KEY("parent_symbol_id") REFERENCES "symbol"("id")
);

CREATE VIEW symbolDefinition (symbol_id, file_id, line, col, flags) AS
  SELECT symbol_id, file_id, line, col, flags
  FROM symbolReference WHERE isDefinition = 1;

CREATE TABLE "symbolDeclaration" (
  "symbol_id"                     INTEGER NOT NULL,
  "file_id"                       INTEGER NOT NULL,
  "startPosition"                 INTEGER NOT NULL,
  "endPosition"                   INTEGER NOT NULL,
  "isDefinition"                  INTEGER NOT NULL DEFAULT 0,
  startPositionLine               INT GENERATED ALWAYS AS (startPosition >> 12) VIRTUAL,
  startPositionColumn             INT GENERATED ALWAYS AS (startPosition & 4095) VIRTUAL,
  endPositionLine                 INT GENERATED ALWAYS AS (endPosition >> 12) VIRTUAL,
  endPositionColumn               INT GENERATED ALWAYS AS (endPosition & 4095) VIRTUAL,
  FOREIGN KEY("symbol_id")        REFERENCES "symbol"("id"),
  FOREIGN KEY("file_id")          REFERENCES "file"("id")
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

CREATE TABLE "argumentPassedByReference" (
  "file_id"               INTEGER NOT NULL,
  "line"                  INTEGER NOT NULL,
  "column"                INTEGER NOT NULL,
  FOREIGN KEY("file_id")  REFERENCES "file"("id")
);

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
    setProperty("database.schema.version", DatabaseSchemaVersion);
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

void SnapshotWriter::setProperty(const std::string& key, int value)
{
  setProperty(key, std::to_string(value));
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
  sql::Statement m_macroInfo;
  sql::Statement m_namespaceAliasInfo;
  sql::Statement m_enumInfo;
  sql::Statement m_enumConstantInfo;
  sql::Statement m_functionInfo;
  sql::Statement m_parameterInfo;
  sql::Statement m_variableInfo;
  const IndexerSymbol* m_currentSymbol = nullptr;

public:
  explicit SymbolExtraInfoInserter(Database& db) : 
    m_macroInfo(db), 
    m_namespaceAliasInfo(db), 
    m_enumInfo(db), 
    m_enumConstantInfo(db), 
    m_functionInfo(db), 
    m_parameterInfo(db), 
    m_variableInfo(db)
  {
    m_macroInfo.prepare("INSERT OR REPLACE INTO macroInfo(id, definition) VALUES(?,?)");
    m_namespaceAliasInfo.prepare("INSERT OR REPLACE INTO namespaceAliasInfo(id, value) VALUES(?,?)");
    m_enumInfo.prepare("INSERT OR REPLACE INTO enumInfo(id, integerType) VALUES(?,?)");
    m_enumConstantInfo.prepare("INSERT OR REPLACE INTO enumConstantInfo(id, value, expression) VALUES(?,?,?)");
    m_functionInfo.prepare("INSERT OR REPLACE INTO functionInfo(id, returnType) VALUES(?,?)");
    m_parameterInfo.prepare("INSERT OR REPLACE INTO parameterInfo(id, parameterIndex, type, defaultValue) VALUES(?,?,?,?)");
    m_variableInfo.prepare("INSERT OR REPLACE INTO variableInfo(id, type, init) VALUES(?,?,?)");
  }

  void process(const std::vector<const IndexerSymbol*>& symbols)
  {
    for (auto sptr : symbols)
    {
      m_currentSymbol = sptr;
      std::visit(*this, m_currentSymbol->extraInfo);
    }
  }

  void operator()(std::monostate)
  {

  }

  void operator()(const MacroInfo& info)
  {
    m_macroInfo.bind(1, m_currentSymbol->id.rawID());
    m_macroInfo.bind(2, std::string_view(info.definition));

    m_macroInfo.insert();
  }

  void operator()(const FunctionInfo& info)
  {
    m_functionInfo.bind(1, m_currentSymbol->id.rawID());
    m_functionInfo.bind(2, std::string_view(info.returnType));

    m_functionInfo.insert();
  }

  void operator()(const ParameterInfo& info)
  {
    m_parameterInfo.bind(1, m_currentSymbol->id.rawID());
    m_parameterInfo.bind(2, info.parameterIndex);
    m_parameterInfo.bind(3, std::string_view(info.type));

    if (!info.defaultValue.empty())
      m_parameterInfo.bind(4, std::string_view(info.defaultValue));
    else
      m_parameterInfo.bind(4, nullptr);

    m_parameterInfo.insert();
  }

  void operator()(const EnumInfo& info)
  {
    m_enumInfo.bind(1, m_currentSymbol->id.rawID());
    m_enumInfo.bind(2, std::string_view(info.underlyingType));

    m_enumInfo.insert();
  }

  void operator()(const EnumConstantInfo& info)
  {
    m_enumConstantInfo.bind(1, m_currentSymbol->id.rawID());
    m_enumConstantInfo.bind(2, info.value);

    if (!info.expression.empty()) {
      m_enumConstantInfo.bind(3, std::string_view(info.expression));
    } else {
      m_enumConstantInfo.bind(3, nullptr);
    }

    m_enumConstantInfo.insert();
  }

  void operator()(const VariableInfo& info)
  {
    m_variableInfo.bind(1, m_currentSymbol->id.rawID());
    m_variableInfo.bind(2, std::string_view(info.type));

    if (!info.init.empty())
      m_variableInfo.bind(3, std::string_view(info.init));
    else
      m_variableInfo.bind(3, nullptr);

    m_variableInfo.insert();
  }

  void operator()(const NamespaceAliasInfo& info)
  {
    m_namespaceAliasInfo.bind(1, m_currentSymbol->id.rawID());
    m_namespaceAliasInfo.bind(2, std::string_view(info.value));

    m_namespaceAliasInfo.insert();
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

void SnapshotWriter::updateSymbolsFlags(const std::vector<const IndexerSymbol*>& symbols)
{
  if (symbols.empty()) {
    return;
  }

  sql::Statement stmt{ 
    database(),
    "UPDATE symbol SET flags = ? WHERE id = ?" };

  for (auto sptr : symbols)
  {
    const IndexerSymbol& sym = *sptr;

    stmt.bind(1, sym.flags);
    stmt.bind(2, sym.id.rawID());

    stmt.update();
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

void SnapshotWriter::insert(const std::vector<ArgumentPassedByReference>& refargs)
{
  if (refargs.empty()) {
    return;
  }

  sql::Statement stmt{ 
    database(),
    "INSERT OR IGNORE INTO argumentPassedByReference(file_id, line, column) VALUES(?,?,?)" 
  };

  for (const ArgumentPassedByReference& refarg : refargs)
  {
    stmt.bind(1, static_cast<int>(refarg.fileID));
    stmt.bind(2, refarg.position.line());
    stmt.bind(3, refarg.position.column());

    stmt.insert();
  }
}

void SnapshotWriter::insert(const std::vector<SymbolDeclaration>& declarations)
{
  if (declarations.empty()) {
    return;
  }

  // "insert or ignore" wouldn't work here because there are no primary key in the sql table.
  sql::Statement stmt{ 
    database(),
    "INSERT INTO symbolDeclaration(symbol_id, file_id, startPosition, endPosition, isDefinition) VALUES(?,?,?,?,?)" 
  };

  for (const SymbolDeclaration& decl : declarations)
  {
    stmt.bind(1, decl.symbolID.rawID());
    stmt.bind(2, static_cast<int>(decl.fileID));
    stmt.bind(3, decl.startPosition.bits());
    stmt.bind(4, decl.endPosition.bits());
    stmt.bind(5, decl.isDefinition);

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

std::vector<SymbolDeclaration> SnapshotWriter::loadDeclarationsInFile(FileID fid)
{
  sql::Statement stmt{ 
    database(),
    "SELECT symbol_id, startPosition, endPosition, isDefinition FROM symbolDeclaration WHERE file_id = ?"
  };

  stmt.bind(1, (int)fid);

  auto read_row = [fid](sql::Statement& q) -> SymbolDeclaration {
    SymbolDeclaration d;
    d.fileID = fid;
    d.symbolID = SymbolID::fromRawID(q.columnInt64(0));
    d.startPosition = FilePosition::fromBits(q.columnInt(1));
    d.endPosition = FilePosition::fromBits(q.columnInt(2));
    d.isDefinition = q.columnInt(3) != 0;
    return d;
    };

  return sql::readRowsAsVector<SymbolDeclaration>(stmt, read_row);
}

void SnapshotWriter::removeAllDeclarationsInFile(FileID fid)
{
  sql::Statement stmt{ 
    database(),
    "DELETE FROM symbolDeclaration WHERE fileID = ?"
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

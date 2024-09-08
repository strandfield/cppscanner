// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_SQL_H
#define CPPSCANNER_SQL_H

#include "database.h"

#include <sqlite3.h>

#include <string>
#include <string_view>

namespace sql
{

using Database = cppscanner::Database;

struct Blob
{
  const char* bytes = nullptr;
  size_t len = 0;

  explicit Blob(const std::string& str) :
    bytes(str.c_str()),
    len(str.size())
  {

  }
};

class Like
{
  std::string m_value;
public:
  explicit Like(const char* val) : m_value(val) { }
  explicit Like(std::string val) : m_value(std::move(val)) { }

  const std::string& str() const { return m_value; }
};

/**
 * \brief wrapper for a SQLite statement
 */
class Statement
{
private:
  Database& m_database;
  sqlite3_stmt* m_statement = nullptr;

public:
  explicit Statement(Database& db);
  Statement(Database& db, const char* query);
  Statement(const Statement&) = delete;
  Statement(Statement&& other);
  ~Statement();

  Database& database() const;

  bool prepare(const char* query);
  int step();
  void reset();
  void finalize();

  bool fetchNextRow();
  void insert();

  std::string errormsg() const;
  int rowid() const;

  void bind(int n, std::nullptr_t);
  void bind(int n, const char* text);
  void bind(int n, std::string&& text);
  void bind(int n, const std::string& text) = delete;
  void bind(int n, std::string_view text);
  void bind(int n, int value);
  void bind(int n, uint64_t value);
  void bind(int n, Blob blob);
  [[deprecated]] void bindBlob(int n, const std::string& bytes);

  bool nullColumn(int n) const;
  std::string column(int n) const;
  int columnInt(int n) const;
  int64_t columnInt64(int n) const;
};

inline Statement::Statement(Database& db)
  : m_database(db)
{

}

inline Statement::Statement(Database& db, const char* query)
  : m_database(db)
{
  prepare(query);
}

inline Statement::Statement(Statement&& other) :
  m_database(other.m_database),
  m_statement(other.m_statement)
{
  other.m_statement = nullptr;
}

inline Statement::~Statement()
{
  finalize();
}

inline Database& Statement::database() const
{
  return m_database;
}

inline bool Statement::prepare(const char* query)
{
  int r = sqlite3_prepare_v2(m_database.sqliteHandle(), query, -1, &m_statement, nullptr);
  return r == SQLITE_OK;
}

inline int Statement::step()
{
  return sqlite3_step(m_statement);
}

inline void Statement::reset()
{
  sqlite3_reset(m_statement);
}

inline void Statement::finalize()
{
  sqlite3_finalize(m_statement);
  m_statement = nullptr;
}

inline bool Statement::fetchNextRow()
{
  int r = step();
  return r == SQLITE_ROW;
}

inline void Statement::insert()
{
  step();
  reset();
}

inline std::string Statement::errormsg() const
{
  return sqlite3_errmsg(m_database.sqliteHandle());
}

inline int Statement::rowid() const
{
  return (int)sqlite3_last_insert_rowid(m_database.sqliteHandle());
}

inline void Statement::bind(int n, std::nullptr_t)
{
  sqlite3_bind_null(m_statement, n);
}

inline void Statement::bind(int n, const char* text)
{
  sqlite3_bind_text(m_statement, n, text, -1, nullptr);
}

inline void Statement::bind(int n, std::string&& text)
{
  sqlite3_bind_text(m_statement, n, text.c_str(), -1, SQLITE_TRANSIENT);
}

inline void Statement::bind(int n, std::string_view text)
{
  sqlite3_bind_text(m_statement, n, text.data(), (int)text.size(), SQLITE_STATIC);
}

inline void Statement::bind(int n, int value)
{
  sqlite3_bind_int(m_statement, n, value);
}

inline void Statement::bind(int n, uint64_t value)
{
  sqlite3_bind_int64(m_statement, n, static_cast<int64_t>(value));
}

inline void Statement::bind(int n, Blob blob)
{
  sqlite3_bind_blob(m_statement, n, blob.bytes, (int)blob.len, nullptr);
}

inline void Statement::bindBlob(int n, const std::string& bytes)
{
  sqlite3_bind_blob(m_statement, n, bytes.c_str(), (int)bytes.size(), nullptr);
}

inline bool Statement::nullColumn(int n) const
{
  return sqlite3_column_type(m_statement, n) == SQLITE_NULL;
}

inline std::string Statement::column(int n) const
{
  const char* str = reinterpret_cast<const char*>(sqlite3_column_text(m_statement, n));
  return str ? std::string(str) : std::string();
}

inline int Statement::columnInt(int n) const
{
  return sqlite3_column_int(m_statement, n);
}

inline int64_t Statement::columnInt64(int n) const
{
  return sqlite3_column_int64(m_statement, n);
}

/*********************************************/

inline bool exec(Database& db, const std::string& query, std::string* error = nullptr)
{
  char* errorbuffer = nullptr;
  int r = sqlite3_exec(db.sqliteHandle(), query.c_str(), NULL, NULL, &errorbuffer);

  if (r != SQLITE_OK && error)
  {
    *error = errorbuffer;
    sqlite3_free(errorbuffer);
  }

  return r == SQLITE_OK;
}

} // namespace sql

#endif // CPPSCANNER_SQL_H

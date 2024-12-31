// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_DATABASE_H
#define CPPSCANNER_DATABASE_H

#include <filesystem>

typedef struct sqlite3 sqlite3;

namespace cppscanner
{

/**
 * \brief wrapper for a SQLite database connection
 * 
 * A default constructed Database corresponds to no connection 
 * with good() returning false.
 * Use open() or create() to open a connection.
 * 
 * The Database class automatically closes the connection (if any) 
 * upon destruction.
 */
class Database
{
public:
  Database() = default;
  Database(const Database&) = delete;
  Database(Database&& other) noexcept;
  ~Database();

  sqlite3* sqliteHandle() const;

  bool good() const;

  bool open(const std::filesystem::path& dbPath);

  void create(const std::filesystem::path& dbPath);

  void close();

  Database& operator=(const Database&) = delete;
  Database& operator=(Database&& other) noexcept;

private:
  sqlite3* m_database = nullptr;
};

} // namespace cppscanner

#endif // CPPSCANNER_DATABASE_H

// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#include "database.h"

#include "sql.h"

#include <cassert>
#include <filesystem>
#include <iostream>

namespace cppscanner
{

Database::Database(Database&& other) noexcept :
  m_database(other.m_database)
{
  other.m_database = nullptr;
}

Database::~Database()
{
  if (sqliteHandle())
    close();
}

/**
 * \brief return the underlying sqlite connection 
 */
sqlite3* Database::sqliteHandle() const
{
  return m_database;
}

/**
 * \brief returns whether an actual database connection is opened
 */
bool Database::good() const
{
  return sqliteHandle() != nullptr;
}

/**
 * \brief opens a connection to a database
 * \param dbPath  the path of the database
 * \return true on success, false otherwise
 */
bool Database::open(const std::filesystem::path& dbPath)
{
  int r = sqlite3_open(dbPath.u8string().c_str(), &m_database);
  return r == SQLITE_OK;
}

/**
 * \brief create a database and open a connection
 * \param dbPath  the path of the database
 */
void Database::create(const std::filesystem::path& dbPath)
{
  if (std::filesystem::exists(dbPath))
    return;

  int r = sqlite3_open_v2(dbPath.u8string().c_str(), &m_database, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);

  assert(r == SQLITE_OK);

  if (r != SQLITE_OK)
  {
    std::cerr << "Failed to create database: error " << SQLITE_OK << std::endl;
  }
}

/**
 * \brief close the connection, if any
 */
void Database::close()
{
  if (!good())
    return;

  sqlite3_close(m_database);
  m_database = nullptr;
}

Database& Database::operator=(Database&& other) noexcept
{
  m_database = other.m_database;
  other.m_database = nullptr;
  return *this;
}

} // namespace cppscanner

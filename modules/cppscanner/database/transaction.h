// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_TRANSACTION_H
#define CPPSCANNER_TRANSACTION_H

#include "sql.h"

namespace sql
{

class Transaction
{
private:
  Database* m_database = nullptr;
  bool m_closed = false;

public:
  explicit Transaction(Database& db);
  Transaction(const Transaction&) = delete;
  Transaction(Transaction&&) = default;
  ~Transaction();

  void commit();

  Transaction& operator=(Transaction&&) = default;
};


/**
* \brief opens a transaction on a database
* \param db  the database
* 
* The database object must outlive the transaction object being constructed.
*/
inline Transaction::Transaction(Database& db) :
  m_database(&db)
{
  sql::exec(*m_database, "BEGIN TRANSACTION");
}

/**
* \brief destroys the transaction object
* 
* This calls commit().
*/
inline Transaction::~Transaction()
{
  commit();
}

/**
* \brief commits the transaction
*/
inline void Transaction::commit()
{
  if (!m_closed)
  {
    sql::exec(*m_database, "COMMIT");
    m_closed = true;
  }
}

/**
 * \brief RAII class for sql transaction
 * 
 * This class can be used to automatically open and close a transaction 
 * in a C++ scope.
 */
class TransactionScope
{
private:
  Transaction m_transaction;

public:
  explicit TransactionScope(Database& db);
  TransactionScope(const TransactionScope&) = delete;
  TransactionScope(TransactionScope&&) = delete;
  ~TransactionScope();

  Transaction& transaction();
};

inline TransactionScope::TransactionScope(Database& db) :
  m_transaction(db)
{

}

inline TransactionScope::~TransactionScope()
{
  transaction().commit();
}

/**
* \brief returns a reference to the transaction object
*/
inline Transaction& TransactionScope::transaction()
{
  return m_transaction;
}

template<typename F>
void runTransacted(Database& db, F&& f)
{
  {
    TransactionScope tr{ db };
    f();
  }
}

} // namespace sql

#endif // CPPSCANNER_TRANSACTION_H

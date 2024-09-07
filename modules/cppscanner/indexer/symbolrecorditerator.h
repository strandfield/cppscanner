// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_SYMBOLRECORDITERATOR_H
#define CPPSCANNER_SYMBOLRECORDITERATOR_H

#include "cppscanner/indexer/snapshot.h"

#include "cppscanner/database/sql.h"

#include <optional>

namespace cppscanner
{

class SymbolRecordFilter
{
public:
  std::optional<SymbolKind> symbolKind;
  std::optional<SymbolID> symbolId;
  std::optional<SymbolID> parentId;
  std::optional<std::string> name;
  bool nameLike = false;

public:
  SymbolRecordFilter() = default;
  SymbolRecordFilter(const SymbolRecordFilter&) = default;

  SymbolRecordFilter& ofKind(SymbolKind k)
  {
    this->symbolKind = k;
    return *this;
  }

  SymbolRecordFilter& withId(SymbolID id)
  {
    this->symbolId = id;
    return *this;
  }

  SymbolRecordFilter& withParent(SymbolID id)
  {
    this->parentId = id;
    return *this;
  }

  SymbolRecordFilter& withName(std::string name)
  {
    this->name = std::move(name);
    this->nameLike = false;
    return *this;
  }

  SymbolRecordFilter& withNameLike(std::string name)
  {
    this->name = std::move(name);
    this->nameLike = true;
    return *this;
  }

  bool empty() const
  {
    return !this->symbolKind.has_value() &&
      !this->parentId.has_value() &&
      !this->name.has_value();
  }
};

class SymbolRecordIterator
{
private:
  sql::Statement m_stmt;
  bool m_has_next = false;

public:
  explicit SymbolRecordIterator(const Snapshot& s, const SymbolRecordFilter& filter = {});
  SymbolRecordIterator(const Snapshot& s, SymbolID id);

  typedef SymbolRecord value_t;

  bool hasNext() const;
  SymbolRecord next();

protected:
  SymbolRecordIterator(sql::Statement&& stmt);

  sql::Statement& stmt();

  static void fill(SymbolRecord& record, sql::Statement& row);

  static sql::Statement build_query(const Snapshot& s, std::string q, const SymbolRecordFilter& f);
  static sql::Statement build_query(const Snapshot& s, std::string q, SymbolID id);
  void advance();
};

inline SymbolRecordIterator::SymbolRecordIterator(const Snapshot& s, const SymbolRecordFilter& filter) : 
  m_stmt(build_query(s, "SELECT id, kind, parent, name, flags FROM symbol", filter))
{
  advance();
}

inline SymbolRecordIterator::SymbolRecordIterator(const Snapshot& s, SymbolID id) : 
  m_stmt(build_query(s, "SELECT id, kind, parent, name, flags FROM symbol", id))
{
  advance();
}

inline SymbolRecordIterator::SymbolRecordIterator(sql::Statement&& stmt) :
  m_stmt(std::move(stmt))
{
  advance();
}

inline bool SymbolRecordIterator::hasNext() const
{
  return m_has_next;
}

inline SymbolRecord SymbolRecordIterator::next()
{
  SymbolRecord r;
  fill(r, m_stmt);
  advance();
  return r;
}

inline sql::Statement& SymbolRecordIterator::stmt()
{
  return m_stmt;
}

inline void SymbolRecordIterator::fill(SymbolRecord& record, sql::Statement& row)
{
  record.id = SymbolID::fromRawID(row.columnInt64(0));
  record.kind = static_cast<SymbolKind>(row.columnInt(1));
  record.parentId = SymbolID::fromRawID(row.columnInt64(2));
  record.name = row.column(3);
  record.flags = row.columnInt(4);
}

inline sql::Statement SymbolRecordIterator::build_query(const Snapshot& s, std::string q, const SymbolRecordFilter& f)
{
  sql::Statement stmt{ s.database() };

  if (!f.empty())
  {
    std::string separator = " ";

    q += " WHERE";

    if (f.symbolId.has_value()) {
      q += separator;
      q += "id = ?";
      separator = " AND ";
    }

    if (f.symbolKind.has_value()) {
      q += separator;
      q += "kind = ?";
      separator = " AND ";
    }

    if (f.parentId.has_value()) {
      q += separator;
      q += "parent = ?";
      separator = " AND ";
    }

    if (f.name.has_value()) {
      q += separator;
      if (f.nameLike) {
        q += "name LIKE ?";
      } else {
        q += "name = ?";
      }
    }
  }

  stmt.prepare(q.c_str());

  {
    int binding_point = 0;

    if (f.symbolId.has_value()) {
      stmt.bind(++binding_point, f.symbolId->rawID());
    }

    if (f.symbolKind.has_value()) {
      stmt.bind(++binding_point, static_cast<int>(*f.symbolKind));
    }

    if (f.parentId.has_value()) {
      stmt.bind(++binding_point, f.parentId->rawID());
    }

    if (f.name.has_value()) {
      stmt.bind(++binding_point, std::string(*f.name));
    }
  }

  return stmt;
}

inline sql::Statement SymbolRecordIterator::build_query(const Snapshot& s, std::string q, SymbolID id)
{
  sql::Statement stmt{ s.database() };
  q += " WHERE id = ?";
  stmt.prepare(q.c_str());
  stmt.bind(1, id.rawID());
  return stmt;
}

inline void SymbolRecordIterator::advance()
{
  m_has_next = m_stmt.fetchNextRow();
}


class EnumConstantRecordIterator : public SymbolRecordIterator
{
public:
  explicit EnumConstantRecordIterator(const Snapshot& s, SymbolRecordFilter filter = {})
    : SymbolRecordIterator(build_query(s, "SELECT id, kind, parent, name, flags, value FROM symbol", filter.ofKind(SymbolKind::EnumConstant)))
  {

  }

  typedef EnumConstantRecord value_t;

  EnumConstantRecord next()
  {
    EnumConstantRecord r;
    fill(r, stmt());
    advance();
    return r;
  }

  static void fill(EnumConstantRecord& record, sql::Statement& row)
  {
    SymbolRecordIterator::fill(record, row);
    record.expression = row.column(5);
  }
};

} // namespace cppscanner

#endif // CPPSCANNER_SYMBOLRECORDITERATOR_H

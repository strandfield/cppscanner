// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#pragma once

#include "sql.h"

#include <stdexcept>
#include <vector>

namespace sql
{

template<typename T, typename F>
T readUniqueRow(sql::Statement& stmt, F&& func)
{
  if (!stmt.fetchNextRow())
  {
    throw std::runtime_error("no rows");
  }

  T value{ func(stmt) };

  if (stmt.fetchNextRow())
  {
    throw std::runtime_error("no unique row");
  }

  return value;
}


template<typename ConvFunc, typename ProcessFunc>
void readRowsConverted(sql::Statement& stmt, ConvFunc&& conv, ProcessFunc&& process)
{
  while (stmt.fetchNextRow())
  {
    process(conv(stmt));
  }
}

template<typename T, typename F>
std::vector<T> readRowsAsVector(sql::Statement& stmt, F&& func)
{
  std::vector<T> r;

  while (stmt.fetchNextRow())
  {
    r.push_back(func(stmt));
  }

  return r;
}

} // namespace sql

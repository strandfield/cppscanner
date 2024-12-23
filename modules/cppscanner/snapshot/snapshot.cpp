// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#include "snapshot.h"

#include "indexersymbol.h"

#include "cppscanner/snapshot/symbolrecorditerator.h"

#include "cppscanner/database/readrows.h"
#include "cppscanner/database/transaction.h"

#include "cppscanner/index/symbol.h"

#include <algorithm>

namespace cppscanner
{

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

std::string Snapshot::normalizedPath(std::string p)
{
  return cppscanner::NormalizedPath(std::move(p));
}

} // namespace cppscanner

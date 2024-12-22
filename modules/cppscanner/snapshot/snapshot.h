// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_SNAPSHOT_H
#define CPPSCANNER_SNAPSHOT_H

#include "cppscanner/index/baseof.h"
#include "cppscanner/index/declaration.h"
#include "cppscanner/index/diagnostic.h"
#include "cppscanner/index/file.h"
#include "cppscanner/index/include.h"
#include "cppscanner/index/override.h"
#include "cppscanner/index/refarg.h"
#include "cppscanner/index/reference.h"
#include "cppscanner/index/symbol.h"

#include <filesystem>
#include <map>

namespace cppscanner
{

class Snapshot
{
public:

  class Path
  {
  private:
    std::string m_path;
  public:
    explicit Path(std::string p) : m_path(Snapshot::normalizedPath(std::move(p))) { }
    const std::string& str() const { return m_path; }
  };

  static std::string normalizedPath(std::string p);

  struct Property
  {
    std::string name;
    std::string value;
  };

  using Properties = std::map<std::string, std::string>;
};

namespace snapshot
{

} // namespace snapshot

} // namespace cppscanner

#endif // CPPSCANNER_SNAPSHOT_H

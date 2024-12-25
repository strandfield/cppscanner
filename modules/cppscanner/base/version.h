// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_VERSION_H
#define CPPSCANNER_VERSION_H

#define CPPSCANNER_VERSION_MAJOR 0
#define CPPSCANNER_VERSION_MINOR 5
#define CPPSCANNER_VERSION_PATCH 0

#define CPPSCANNER_VERSION_STRING "0.5.0-dev"

namespace cppscanner
{

inline constexpr const char* versioncstr()
{
  return CPPSCANNER_VERSION_STRING;
}

} // namespace cppscanner

#endif // CPPSCANNER_VERSION_H

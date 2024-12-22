// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_OS_H
#define CPPSCANNER_OS_H

namespace cppscanner
{

inline constexpr const char* system_name()
{
#ifdef _WIN32
  return "windows";
#else
  return "linux";
#endif // _WIN32

}

} // namespace cppscanner

#endif // CPPSCANNER_OS_H

// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_ACCESS_H
#define CPPSCANNER_ACCESS_H

#include <string_view>

namespace cppscanner
{

enum class AccessSpecifier
{
  Invalid = 0,
  Public,
  Protected,
  Private,
};

inline std::string_view getAccessSpecifierString(AccessSpecifier aspec)
{
  switch (aspec)
  {
  case AccessSpecifier::Public:
    return "public";
  case AccessSpecifier::Protected:
    return "protected";
  case AccessSpecifier::Private:
    return "private";
  default:
    return "<invalid>";
  }
}

template<typename F>
void enumerateAccessSpecifier(F&& fn)
{
  fn(AccessSpecifier::Public);
  fn(AccessSpecifier::Protected);
  fn(AccessSpecifier::Private);
  fn(AccessSpecifier::Invalid);
}

} // namespace cppscanner

#endif // CPPSCANNER_ACCESS_H

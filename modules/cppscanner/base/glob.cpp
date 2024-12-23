// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#include "glob.h"

#include <algorithm>
#include <regex>

namespace cppscanner
{

bool is_glob_pattern(const std::string& string)
{
  return std::any_of(string.begin(), string.end(), [](char c) {
    return c == '/' || c == '?' || c == '*';
    }) || std::none_of(string.begin(), string.end(), [](char c) {
      return c == '.';
      });
}

// poor man's glob-to-regex conversion.
// should be ok for most use case
std::regex glob2regex(const std::string& pattern)
{
  std::string r;
  r.reserve(pattern.size() + 7);

  for (char c : pattern)
  {
    if (c == '.')
    {
      r.push_back('\\');
      r.push_back('.');
    }
    else if (c == '?')
    {
      r.push_back('.');
    }
    else if (c == '*')
    {
      r.push_back('.');
      r.push_back('*');
    }
#ifdef WIN32
    else if (c == '\\' || c == '/')
    {
      r += "[\\\\\\/]";
    }
#endif // WIN32
    else
    {
      r.push_back(c);
    }
  }

  return std::regex(r);
}

bool glob_match(const std::string& input, const std::string& pattern)
{
  std::regex r = glob2regex(pattern);
  return std::regex_search(input, r);
}

} // namespace cppscanner

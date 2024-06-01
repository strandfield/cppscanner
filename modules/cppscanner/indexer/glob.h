// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_GLOB_H
#define CPPSCANNER_GLOB_H

#include <string>

namespace cppscanner
{

bool is_glob_pattern(const std::string& string);
bool glob_match(const std::string& input, const std::string& pattern);

} // namespace cppscanner

#endif // CPPSCANNER_GLOB_H

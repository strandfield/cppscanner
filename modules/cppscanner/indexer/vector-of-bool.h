// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#pragma once

#include <vector>

namespace cppscanner
{

inline void set_flag(std::vector<bool>& flags, size_t i)
{
  if (flags.size() <= i) {
    flags.resize(i + 1, false);
  }

  flags[i] = true;
}

inline bool test_flag(const std::vector<bool>& flags, size_t i)
{
  return i < flags.size() && flags.at(i);
}

} // namespace cppscanner

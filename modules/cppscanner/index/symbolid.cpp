// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#include "symbolid.h"

#include <algorithm>
#include <array>

namespace cppscanner
{

static const std::array<char, 16> hexDigits = {
  '0', '1', '2', '3', '4', '5', '6',
  '7', '8', '9', 'a', 'b', 'c', 'd',
  'e', 'f'
};

std::string SymbolID::toHex() const
{
  std::string result;
  result.reserve(16);

  uint64_t v = rawID();
  uint64_t sixteen = 16;

  while (result.size() < 16) {
    result.push_back(hexDigits[v % sixteen]);
    v /= sixteen;
  }

  std::reverse(result.begin(), result.end());

  return result;
}

} // namespace cppscanner

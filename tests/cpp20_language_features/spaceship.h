
#pragma once

#include <compare>

struct IntPair
{
  int a;
  int b;

  auto operator<=>(const IntPair&) const = default;
};

IntPair make_intpair(int a, int b);

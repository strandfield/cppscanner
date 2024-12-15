
#include "spaceship.h"

int ret = 0;

void check(bool v)
{
  if (!v) ++ret;
}

int main()
{
  IntPair a = make_intpair(1, 2);
  IntPair b{ .a = 2, .b = 4, };

  check(a != b);
  check(a < b);

  return ret;
}

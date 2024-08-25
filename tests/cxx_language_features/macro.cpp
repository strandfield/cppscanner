
#include "macro.h"

int incrByConstant(int& n)
{
  n += MY_CONSTANT;
}

bool isGreaterThanConstant(int n)
{
  return GREATER_THAN_MY_CONSTANT(n);
}

int min_int(int a, int b)
{
  return a < b ? a : b;
}

#undef MY_MIN

#define MY_MIN(a, b) min_int(a, b)

int min_with_constant(int n)
{
  return MY_MIN(n, MY_CONSTANT);
}

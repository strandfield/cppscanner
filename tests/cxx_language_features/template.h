
#pragma once

template<typename T>
const T& max(const T& lhs, const T& rhs)
{
  return lhs < rhs ? rhs : lhs;
}

template<int N>
int incr(int n)
{
  return n + N;
}

struct add
{
  template<typename T, typename U>
  auto operator()(const T& lhs, const U& rhs) -> decltype(lhs + rhs)
  {
    return lhs + rhs;
  }
};

template<typename T, typename U>
struct is_same
{
  static constexpr bool value = false;
};

template<typename T>
struct is_same<T, T>
{
  static constexpr bool value = true;
};

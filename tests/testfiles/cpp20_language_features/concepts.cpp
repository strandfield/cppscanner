
#include "concepts.h"

#include <concepts>

#include <vector>
#include <string>

#include <iostream>

template<typename T>
concept Number = std::integral<T> || std::floating_point<T>;

template<typename T>
concept Iterable = requires(T val) {
  val.begin();
  val.end();
  val.insert(val.end(), val.begin(), val.end());
};

Number auto repeated(const Number auto& x, int n)
{
  return x * static_cast<decltype(x)>(n);
}

template<Iterable T>
T repeated(const T& x, int n)
{
  T result;

  while (n > 0)
  {
    result.insert(result.end(), x.begin(), x.end());
    --n;
  }

  return result;
}

void testConcepts()
{
  std::cout << repeated(2, 4) << std::endl;
  std::cout << repeated(3.1, 3) << std::endl;
  std::cout << repeated(std::string("a"), 3) << std::endl;
  std::cout << repeated(std::string("b"), 0) << std::endl;

  std::vector<int> list = repeated(std::vector<int>{1, 2, 3}, 3);
  std::cout << list.size() << std::endl;
}


#include <tuple>

void hello_tuple()
{
  auto mytuple = std::make_tuple(1, 3.14f, true);
  
  auto n = std::get<int>(mytuple);
  auto x = std::get<float>(mytuple);
  auto b = std::get<bool>(mytuple);

  // structured binding
  auto [t1, t2, t3] = mytuple;
}

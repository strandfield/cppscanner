
#include <tuple>

void hello_tuple()
{
  auto tuple = std::make_tuple(1, 3.14f, true);
  
  auto n = std::get<int>(tuple);
  auto x = std::get<float>(tuple);
  auto b = std::get<bool>(tuple);

  // structured binding
  auto [t1, t2, t3] = tuple;
}

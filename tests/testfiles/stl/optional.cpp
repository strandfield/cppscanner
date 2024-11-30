
#include <optional>

int hello_optional(int a, std::optional<int> b)
{
  return a + b.value_or(1);
}

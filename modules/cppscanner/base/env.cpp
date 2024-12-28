#include "env.h"

#include <cstdlib>

namespace cppscanner
{

std::optional<std::string> readEnv(const char* varName)
{
  if (const char* val = std::getenv(varName))
  {
    return std::string(val);
  }
  else
  {
    return std::nullopt;
  }
}

} // namespace cppscanner

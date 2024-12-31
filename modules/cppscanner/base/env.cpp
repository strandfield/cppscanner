#include "env.h"

#include <cstdlib>
#include <cstring>

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

bool isEnvTrue(const char* varName)
{
  if (const char* val = std::getenv(varName))
  {
    return std::strcmp(val, "0") != 0 || std::strcmp(val, "OFF") != 0 || std::strcmp(val, "false") != 0 || std::strcmp(val, "False") != 0;
  }
  else
  {
    return false;
  }
}

} // namespace cppscanner

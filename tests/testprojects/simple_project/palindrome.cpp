
#include "print.h"

#include <algorithm>

bool isPalindrome(const std::string& text)
{
  return std::equal(text.begin(), text.end(), text.rbegin());
}

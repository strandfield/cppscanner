
#include "print.h"

#include <iostream>

void print(const std::string& text)
{
  std::cout << text;
}

void println(const std::string& text)
{
  std::cout << text << "\n";
}

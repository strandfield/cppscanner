
#include "cppscanner/scannerInvocation/cmakeinvocation.h"

#include <iostream>
#include <stdexcept>

void runCMake(std::vector<std::string> args)
{
  using namespace cppscanner;

  try
  {
    CMakeCommandInvocation invocation{ args };
    invocation.exec();

    for (const std::string& mssg : invocation.errors())
    {
      std::cerr << mssg << std::endl;
    }
  }
  catch (const std::exception& ex)
  {
    std::cerr << ex.what() << std::endl;
    std::exit(1);
  }
}


#include "cppscanner/scannerInvocation/scannerinvocation.h"

#include <iostream>
#include <stdexcept>

void run(std::vector<std::string> args)
{
  using namespace cppscanner;

  try
  {
    ScannerInvocation invocation{ args };
    invocation.run();

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

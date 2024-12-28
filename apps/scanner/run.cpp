
#include "cppscanner/scannerInvocation/scannerinvocation.h"

#include <iostream>
#include <stdexcept>


[[noreturn]] void runHelp()
{
  cppscanner::ScannerInvocation::printHelp();
  std::exit(0);
}

void run(std::vector<std::string> args)
{
  using namespace cppscanner;

  if (args.size() < 1 || args.at(0) == "--help" || args.at(0) == "-h") {
    runHelp();
  }

  try
  {
    ScannerInvocation invocation;
    invocation.parseCommandLine(args);
    invocation.parseEnv();
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


#include "cppscanner/scannerInvocation/mergeinvocation.h"

#include <iostream>
#include <stdexcept>

[[noreturn]] void mergeHelp()
{
  cppscanner::MergeCommandInvocation::printHelp();
  std::exit(0);
}


void runMerge(std::vector<std::string> args)
{
  using namespace cppscanner;

  if (args.size() < 1 || args.at(0) == "--help" || args.at(0) == "-h") {
    mergeHelp();
  }

  try
  {
    MergeCommandInvocation invocation;
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

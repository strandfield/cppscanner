
#include "cppscanner/base/version.h"

#include "cppscanner/scannerInvocation/scannerinvocation.h"

#include <iostream>
#include <string>
#include <vector>

[[noreturn]] void version()
{
  std::cout << cppscanner::versioncstr() << std::endl;
  std::exit(0);
}

[[noreturn]] void help()
{
  cppscanner::ScannerInvocation::printHelp();
  std::exit(0);
}

int main(int argc, char* argv[])
{
  auto args = std::vector<std::string>(argv, argv + argc);

  if (args.size() < 2 || args.at(1) == "--help" || args.at(1) == "-h")
    help();
  else if (args.at(1) == "--version" || args.at(1) == "-v")
    version();

  args.erase(args.begin(), args.begin() + 1);
  cppscanner::ScannerInvocation invocation;
  try 
  {
    invocation.parseCommandLine(args);
  }
  catch (const std::exception& ex)
  {
    std::cout << ex.what() << std::endl;
    return 1;
  }
  invocation.parseEnv();

  if (!invocation.run())
  {
    for (const std::string& message : invocation.errors())
    {
      std::cout << message << std::endl;
    }

    return 1;
  }

  return 0;
}

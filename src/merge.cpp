
#include "cppscanner/scannerInvocation/mergeinvocation.h"

#include <iostream>
#include <stdexcept>

constexpr const char* MERGE_DESCRIPTION = R"(Description:
  Merge two or more snapshots into one.)";


[[noreturn]] void mergeHelp()
{
  std::cout << "Syntax:" << std::endl;
  std::cout << "  cppscanner merge -o <output> input1 input2 ..." << std::endl;
  std::cout << "" << std::endl;
  std::cout << MERGE_DESCRIPTION << std::endl;

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
    MergeCommandInvocation invocation{ args };
    invocation.readEnv();
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

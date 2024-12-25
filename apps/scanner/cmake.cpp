
#include "cppscanner/scannerInvocation/cmakeinvocation.h"

#include <iostream>
#include <stdexcept>

constexpr const char* CMAKE_DESCRIPTION = R"(Description:
  Generates a project with CMake.
  The command line arguments are passed as-is to the CMake executable.
  The CMake file-based API is used by cppscanner to get information
  about the project (targets, toolchains) so that the CMake build
  directory can be used as input to the 'run' command.)";


[[noreturn]] void cmakeHelp()
{
  std::cout << "Syntax:" << std::endl;
  std::cout << "  cppscanner cmake -B <build-directory> -S <source-directory> [cmake-options]" << std::endl;
  std::cout << "  cppscanner cmake [cmake-options] <source-or-build-directory" << std::endl;
  std::cout << "" << std::endl;
  std::cout << CMAKE_DESCRIPTION << std::endl;

  std::exit(0);
}


void runCMake(std::vector<std::string> args)
{
  using namespace cppscanner;

  if (args.size() < 1 || args.at(0) == "--help" || args.at(0) == "-h") {
    cmakeHelp();
  }

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

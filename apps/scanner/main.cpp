
#include "cppscanner/base/version.h"

#include <iostream>
#include <string>
#include <vector>

extern void run(std::vector<std::string> args);
extern void runMerge(std::vector<std::string> args);

[[noreturn]] void version()
{
  std::cout << cppscanner::versioncstr() << std::endl;
  std::exit(0);
}

[[noreturn]] void help()
{
  // TODO: replace with   ScannerInvocation::printHelp()

  std::cout << "cppscanner is a clang-based command-line utility to create snapshots of C++ programs." << std::endl;
  std::cout << std::endl;
  std::cout << "Commands:" << std::endl;
  std::cout << "  run: runs the scanner to create a snapshot" << std::endl;
  std::cout << "  merge: merge two or more snapshots" << std::endl;
  std::cout << std::endl;
  std::cout << "Use the '-h' option to get more information about each command." << std::endl;
  std::cout << "Example: cppscanner run -h" << std::endl;

  std::exit(0);
}

int main(int argc, char* argv[])
{
  auto args = std::vector<std::string>(argv, argv + argc);

  if (args.size() < 2 || args.at(1) == "--help" || args.at(1) == "-h")
    help();
  else if (args.at(1) == "--version" || args.at(1) == "-v")
    version();

  if (args.at(1) == "run")
  {
    args.erase(args.begin(), args.begin() + 1);
    run(args);
  }
  else if (args.at(1) == "merge")
  {
    args.erase(args.begin(), args.begin() + 2);
    runMerge(args);
  }
  else
  {
    std::cerr << "unrecognized command " << args.at(1) << std::endl;
    return 1;
  }
}

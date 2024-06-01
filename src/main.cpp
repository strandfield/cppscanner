
#include "cppscanner/indexer/version.h"

#include <iostream>
#include <string>
#include <vector>

extern void run(std::vector<std::string> args);

[[noreturn]] void version()
{
  std::cout << cppscanner::versioncstr() << std::endl;
  std::exit(0);
}

[[noreturn]] void help()
{
  std::cout << "cppscanner is a clang-based command-line utility to create snapshots of C++ programs." << std::endl;
  std::cout << std::endl;
  std::cout << "Syntax:" << std::endl;
  std::cout << "  cppscanner run --compile-commands <compile_commands.json> --output <snapshot.db>" << std::endl;

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
    args.erase(args.begin(), args.begin() + 2);
    run(args);
  }
  else
  {
    std::cerr << "unrecognized command " << args.at(1) << std::endl;
    return 1;
  }
}

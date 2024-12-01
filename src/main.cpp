
#include "cppscanner/indexer/version.h"

#include <iostream>
#include <string>
#include <vector>

extern void run(std::vector<std::string> args);
extern void runCMake(std::vector<std::string> args);

[[noreturn]] void version()
{
  std::cout << cppscanner::versioncstr() << std::endl;
  std::exit(0);
}

constexpr const char* HELP_OPTIONS = R"(Options:
  -y
  --overwrite             overwrites output file if it exists
  --home <directory>      specifies a home directory
  --root <directory>      specifies a root directory
  --index-external-files  specifies that files outside of the home directory should be indexed
  --index-local-symbols   specifies that local symbols should be indexed
  -f <pattern>
  --filter <pattern>      specifies a pattern for the file to index
  --filter_tu <pattern>
  -f:tu <pattern>         specifies a pattern for the translation units to index
  --threads <count>       number of threads dedicated to parsing translation units
  --project-name <name>   specifies the name of the project
  --project-version <v>   specifies a version for the project)";

constexpr const char* HELP_DESCRIPTION = R"(Description:
  Creates a snapshot of a C++ program by indexing the translation units listed in
  the compile_commands.json file passed as input.
  You may use filters to restrict the files or translation units that are going 
  to be processed.
  If --index-external-files is specified, all files under the root directory will
  be indexed. If no root directory is specified, then all files will be indexed.
  Otherwise, only the files under the home directory are indexed. If no home is 
  specified, it defaults to the current working directory.
  If --index-local-symbols is specified, locals symbol (e.g., variables defined 
  in function bodies) will be indexed.
  Unless a non-zero number of parsing threads is specified, the scanner runs in a
  single-threaded mode.
  The name and version of the project are written as metadata in the snapshot
  if they are provided but are otherwise not used while indexing.)";

constexpr const char* HELP_EXAMPLES = R"(Example:
  Compile a single file with C++17 enabled:
    cppscanner -i source.cpp -o snapshot.db -- -std=c++17)";


[[noreturn]] void help()
{
  std::cout << "cppscanner is a clang-based command-line utility to create snapshots of C++ programs." << std::endl;
  std::cout << std::endl;
  std::cout << "Syntax:" << std::endl;
  std::cout << "  cppscanner run --compile-commands <compile_commands.json> --output <snapshot.db> [options]" << std::endl;
  std::cout << "  cppscanner run -i <source.cpp> --output <snapshot.db> [options] [--] [compilation arguments]" << std::endl;
  std::cout << "" << std::endl;
  std::cout << HELP_OPTIONS << std::endl;
  std::cout << "" << std::endl;
  std::cout << HELP_DESCRIPTION << std::endl;
  std::cout << "" << std::endl;
  std::cout << HELP_EXAMPLES << std::endl;

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
  else if (args.at(1) == "cmake")
  {
    args.erase(args.begin(), args.begin() + 2);
    runCMake(args);
  }
  else
  {
    std::cerr << "unrecognized command " << args.at(1) << std::endl;
    return 1;
  }
}

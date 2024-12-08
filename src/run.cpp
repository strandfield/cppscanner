
#include "cppscanner/scannerInvocation/scannerinvocation.h"

#include <iostream>
#include <stdexcept>


constexpr const char* RUN_OPTIONS = R"(Options:
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

constexpr const char* RUN_DESCRIPTION = R"(Description:
  Creates a snapshot of a C++ program by indexing one or more translation units 
  passed as inputs.
  The different syntaxes specify how the list is computed:
  a) each compile command in compile_commands.json is assumed to represent a 
     translation unit;
  b) the file passed as input is a single translation units;
  c) translation units are extracted from the specified CMake target (the 'cmake'
     command must have been used beforehand).
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

constexpr const char* RUN_EXAMPLES = R"(Example:
  Compile a single file with C++17 enabled:
    cppscanner -i source.cpp -o snapshot.db -- -std=c++17)";

[[noreturn]] void runHelp()
{
  std::cout << "Syntax:" << std::endl;
  std::cout << "  cppscanner run --compile-commands <compile_commands.json> --output <snapshot.db> [options]" << std::endl;
  std::cout << "  cppscanner run -i <source.cpp> --output <snapshot.db> [options] [--] [compilation arguments]" << std::endl;
  std::cout << "  cppscanner run --build <cmake-build-dir> [--config <cmake-config>] [--target <cmake-target>] --output <snapshot.db> [options]" << std::endl;
  std::cout << "" << std::endl;
  std::cout << RUN_OPTIONS << std::endl;
  std::cout << "" << std::endl;
  std::cout << RUN_DESCRIPTION << std::endl;
  std::cout << "" << std::endl;
  std::cout << RUN_EXAMPLES << std::endl;

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

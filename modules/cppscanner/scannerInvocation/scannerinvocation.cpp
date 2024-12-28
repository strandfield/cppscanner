
#include "scannerinvocation.h"

#include "compilecommandsgenerator.h"

#include "cppscanner/base/config.h"
#include "cppscanner/base/env.h"

#include <array>
#include <iostream>
#include <stdexcept>

namespace cppscanner
{

namespace
{

bool isOption(const std::string& arg)
{
  return !arg.empty() && arg.front() == '-';
}

// test if arg is "-j<number>"
bool isJobsArg(const std::string& arg)
{
  if (arg.size() <= 2 || arg.rfind("-j", 0) != 0)
  {
    return false;
  }

  for (size_t i(2); i < arg.size(); ++i)
  {
    char c = arg.at(i);

    if (c < '0' || c > '9')
    {
      return false;
    }
  }

  return true;
}

void checkConsistency(const ScannerOptions& opts)
{
  const std::array<bool, 3> inputTypes = { opts.compile_commands.has_value(),opts.cmakeBuildDirectory.has_value(), !opts.inputs.empty() };
  const size_t nbInputs = std::count(inputTypes.begin(), inputTypes.end(), true);

  if (nbInputs != 1)
  {
    throw std::runtime_error("too many or too few inputs");
  }

  if (opts.output.empty()) {
    throw std::runtime_error("missing output file");
  }

  if (opts.root.has_value() && !std::filesystem::is_directory(*opts.root)) {
    throw std::runtime_error("Root path must be a directory");
  }

}

} // namespace

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

void ScannerInvocation::printHelp()
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
}

ScannerInvocation::ScannerInvocation(const std::vector<std::string>& commandLine)
{
  parseCommandLine(commandLine);
  checkConsistency(m_options);
}

ScannerInvocation::ScannerInvocation()
{

}

void ScannerInvocation::parseCommandLine(const std::vector<std::string>& commandLine)
{
  const auto& args = commandLine;
  auto& result = m_options;

  for (size_t i(0); i < args.size();)
  {
    const std::string& arg = args.at(i++);

    if (arg == "--compile-commands") 
    {
      if (i >= args.size())
        throw std::runtime_error("missing argument after --compile-commands");

      result.compile_commands = args.at(i++);
    }
    else if (arg == "--input" || arg == "-i") 
    {
      if (i >= args.size())
        throw std::runtime_error("missing argument after " + arg + " command");

      result.inputs.push_back(args.at(i++));
    }
    else if (arg == "--build")
    {
      if (i >= args.size())
        throw std::runtime_error("missing argument after --build");

      result.cmakeBuildDirectory = std::filesystem::path(args.at(i++));
    }
    else if (arg == "--config")
    {
      if (i >= args.size())
        throw std::runtime_error("missing argument after --config");

      result.cmakeConfig = args.at(i++);
    }
    else if (arg == "--target")
    {
      if (i >= args.size())
        throw std::runtime_error("missing argument after --target");

      while (i < args.size() && !isOption(args.at(i)))
      {
        result.cmakeTargets.push_back(args.at(i++));
      }
    }
    else if (arg == "--output" || arg == "-o") 
    {
      if (i >= args.size())
        throw std::runtime_error("missing argument after --output");

      result.output = args.at(i++);
    }
    else if (arg == "--home")
    {
      if (i >= args.size())
        throw std::runtime_error("missing argument after --home");

      result.home = std::filesystem::path(args.at(i++));
    }
    else if (arg == "--root")
    {
      if (i >= args.size())
        throw std::runtime_error("missing argument after --root");

      result.root = std::filesystem::path(args.at(i++));
    }
    else if (arg == "--index-external-files")
    {
      result.index_external_files = true;
    }
    else if (arg == "--index-local-symbols")
    {
      result.index_local_symbols = true;
    }
    else if (arg == "--ignore-file-content")
    {
      result.ignore_file_content = true;
    }
    else if (arg == "--remap-file-ids")
    {
      result.remap_file_ids = true;
    }
    else if (arg == "--overwrite" || arg == "-y") 
    {
      result.overwrite = true;
    }
    else if (arg == "--filter" || arg == "-f")
    {
      if (i >= args.size())
        throw std::runtime_error("missing argument after --filter");

      result.filters.push_back(args.at(i++));
    }
    else if (arg == "--filter_tu" || arg == "-f:tu")
    {
      if (i >= args.size())
        throw std::runtime_error("missing argument after --filter_tu");

      result.translation_unit_filters.push_back(args.at(i++));
    }
    else if (arg == "--threads" || arg == "-j" || isJobsArg(arg))
    {
      if (arg == "--threads" || arg == "-j")
      {
        if (i >= args.size())
          throw std::runtime_error("missing argument after " + arg);

        result.nb_threads = std::stoi(args.at(i++));
      }
      else
      {
        result.nb_threads = std::stoi(arg.substr(2));
      }
    }
    else if (arg == "--project-name")
    {
      if (i >= args.size())
        throw std::runtime_error("missing argument after --project-name");

      result.project_name = args.at(i++);
    }
    else if (arg == "--project-version")
    {
      if (i >= args.size())
        throw std::runtime_error("missing argument after --project-version");

      result.project_version = args.at(i++);
    }
    else if (arg == "--")
    {
      result.compilation_arguments.assign(args.begin() + i, args.end());
      i = args.size();
    }
    else
    {
      throw std::runtime_error("unrecognized command line argument: " + arg);
    }
  }
}

void ScannerInvocation::parseEnv()
{
  if (!m_options.home.has_value())
  {
    std::optional<std::string> dir = readEnv(ENV_HOME_DIR);

    if (dir) {
      m_options.home = *dir;
    }
  }

  if (!m_options.project_name.has_value())
  {
    std::optional<std::string> value = readEnv(ENV_PROJECT_NAME);

    if (value) {
      m_options.project_name = *value;
    }
  }

  if (!m_options.project_version.has_value())
  {
    std::optional<std::string> value = readEnv(ENV_PROJECT_VERSION);

    if (value) {
      m_options.project_version = *value;
    }
  }

  if (!m_options.index_local_symbols)
  {
    m_options.index_local_symbols = isEnvTrue(ENV_INDEX_LOCAL_SYMBOLS);
  }
}

const ScannerOptions& ScannerInvocation::options() const
{
  return m_options;
}

bool ScannerInvocation::run()
{
  if (std::filesystem::exists(options().output))
  {
    if (options().overwrite)
    {
      std::filesystem::remove(options().output);
    }
    else
    {
      m_errors.push_back("output file already exists");
      return false;
    }
  }

  Scanner scanner;

  scanner.setOutputPath(options().output);

  if (options().home.has_value()) {
    scanner.setHomeDir(*options().home);
  }

  scanner.setIndexExternalFiles(options().index_external_files);

  if (options().root.has_value()) {
    scanner.setRootDir(*options().root);
  } 

  if (options().index_local_symbols) {
    scanner.setIndexLocalSymbols();
  }

  if (options().ignore_file_content) {
    scanner.setCaptureFileContent(false);
  }

  if (options().remap_file_ids) {
    scanner.setRemapFileIds(true);
  }

  if (!options().filters.empty()) {
    scanner.setFilters(options().filters);
  }

  if (!options().translation_unit_filters.empty()) {
    scanner.setTranslationUnitFilters(options().translation_unit_filters);
  }

  if (options().nb_threads.has_value())
  {
    int n = *options().nb_threads;
    if (n >= 0) {
      scanner.setNumberOfParsingThread((size_t)n);
    }
  }

  if (options().project_name.has_value()) {
    scanner.setExtraProperty(PROPERTY_PROJECT_NAME, *options().project_name);
  }

  if (options().project_version.has_value()) {
    scanner.setExtraProperty(PROPERTY_PROJECT_VERSION, *options().project_version);
  }

  if (options().compile_commands.has_value())
  {
    scanner.scanFromCompileCommands(*options().compile_commands);
  }
  else if (options().cmakeBuildDirectory.has_value())
  {
    // TODO: we could use the cmake source directory to set our Home directory
    std::vector<ScannerCompileCommand> commands = generateCommands(
      *options().cmakeBuildDirectory,
      options().cmakeConfig,
      options().cmakeTargets);
    scanner.scan(commands);
  }
  else
  {
    scanner.scanFromListOfInputs(options().inputs, options().compilation_arguments);
  }

  return true;
}

const std::vector<std::string>& ScannerInvocation::errors() const
{
  return m_errors;
}

} // namespace cppscanner

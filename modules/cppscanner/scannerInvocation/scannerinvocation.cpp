
#include "scannerinvocation.h"

#include "cppscanner/base/config.h"
#include "cppscanner/base/env.h"

#include "cppscanner/snapshot/merge.h"

#include "cppscanner/indexer/scanner.h"

#include <algorithm>
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

class ConsistencyChecker
{
public:

  void operator()(std::monostate)
  {

  }

  void operator()(const ScannerInvocation::RunOptions& opts)
  {
    const std::array<bool, 2> inputTypes = { opts.compile_commands.has_value(), !opts.inputs.empty() };
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

  void operator()(const ScannerInvocation::MergeOptions&)
  {

  }
};

void checkConsistency(const ScannerInvocation::Options& opts)
{
  ConsistencyChecker checker;
  std::visit(checker, opts.command);
}

class InvocationRunner
{
private:
  const ScannerInvocation::Options& m_options;
  std::vector<std::string>& m_errors;

public:
  InvocationRunner(const ScannerInvocation::Options& opts, std::vector<std::string>& errors)
    : m_options(opts)
    , m_errors(errors)
  {
    
  }

  bool run()
  {
    return std::visit(*this, m_options.command);
  }

  const ScannerInvocation::Options& globalOptions() const
  {
    return m_options;
  }

  bool operator()(std::monostate);
  bool operator()(const ScannerInvocation::RunOptions& opts);
  bool operator()(const ScannerInvocation::MergeOptions& opts);
};

bool InvocationRunner::operator()(std::monostate)
{
  if (globalOptions().helpFlag)
  {
    ScannerInvocation::printHelp();
    return true;
  }

  return false;
}

bool InvocationRunner::operator()(const ScannerInvocation::RunOptions& opts)
{
  if (globalOptions().helpFlag)
  {
    ScannerInvocation::printHelp(ScannerInvocation::Command::Run);
    return true;
  }

  if (std::filesystem::exists(opts.output))
  {
    if (opts.overwrite)
    {
      std::filesystem::remove(opts.output);
    }
    else
    {
      m_errors.push_back("output file already exists");
      return false;
    }
  }

  Scanner scanner;

  scanner.setOutputPath(opts.output);

  if (opts.home.has_value()) {
    scanner.setHomeDir(*opts.home);
  }

  scanner.setIndexExternalFiles(opts.index_external_files);

  if (opts.root.has_value()) {
    scanner.setRootDir(*opts.root);
  } 

  if (opts.index_local_symbols) {
    scanner.setIndexLocalSymbols();
  }

  if (opts.ignore_file_content) {
    scanner.setCaptureFileContent(false);
  }

  if (opts.remap_file_ids) {
    scanner.setRemapFileIds(true);
  }

  if (!opts.filters.empty()) {
    scanner.setFilters(opts.filters);
  }

  if (!opts.translation_unit_filters.empty()) {
    scanner.setTranslationUnitFilters(opts.translation_unit_filters);
  }

  if (opts.nb_threads.has_value())
  {
    int n = *opts.nb_threads;
    if (n >= 0) {
      scanner.setNumberOfParsingThread((size_t)n);
    }
  }

  if (opts.project_name.has_value()) {
    scanner.setExtraProperty(PROPERTY_PROJECT_NAME, *opts.project_name);
  }

  if (opts.project_version.has_value()) {
    scanner.setExtraProperty(PROPERTY_PROJECT_VERSION, *opts.project_version);
  }

  if (opts.compile_commands.has_value())
  {
    scanner.scanFromCompileCommands(*opts.compile_commands);
  }
  else
  {
    scanner.scanFromListOfInputs(opts.inputs, opts.compilation_arguments);
  }

  return true;
}


std::vector<std::filesystem::path> listInputFiles(const std::vector<std::filesystem::path>& scannerDirectories)
{
  std::vector<std::filesystem::path> result;

  for (const std::filesystem::path& scannerFolderPath : scannerDirectories)
  {
    for (const std::filesystem::directory_entry& e : std::filesystem::directory_iterator(scannerFolderPath))
    {
      if (e.is_regular_file() && e.path().extension() == PLUGIN_SNAPSHOT_EXTENSION)
      {
        result.push_back(e.path());
      }
    }
  }

  return result;
}

void searchScannerDirectoriesRecursively(std::vector<std::filesystem::path>&output, const std::filesystem::path& folderPath)
{
  if (folderPath.filename() == PLUGIN_OUTPUT_FOLDER_NAME)
  {
    output.push_back(folderPath);
  }
  else
  {
    for (const std::filesystem::directory_entry& e : std::filesystem::recursive_directory_iterator(folderPath))
    {
      if (e.is_directory() && e.path().filename() == PLUGIN_OUTPUT_FOLDER_NAME)
      {
        output.push_back(e.path());
      }
    }
  }
}

std::vector<std::filesystem::path> searchScannerDirectoriesRecursively(const std::vector<std::string>& paths)
{
  std::vector<std::filesystem::path> result;

  if (!paths.empty())
  {
    for (const std::string& path : paths)
    {
      searchScannerDirectoriesRecursively(result, path);
    }
  }
  else
  {    
    std::optional<std::string> outDir = readEnv(ENV_PLUGIN_OUTPUT_DIR);
    if (outDir)
    {
      searchScannerDirectoriesRecursively(result, *outDir);
    }

    if(!outDir || result.empty())
    {
      searchScannerDirectoriesRecursively(result, std::filesystem::current_path().string());
    }
  }

  return result;
}

#ifdef _WIN32
void convertToLocalPath(std::string& path)
{
  if (path.length() < 3 || path.at(0) != '/' || path.at(2) != '/') {
    return;
  }

  path[0] = std::toupper(path.at(1));
  path[1] = ':';

  for (char& c : path)
  {
    if (c == '/')
    {
      c = '\\';
    }
  }
}
#else
void convertToLocalPath(std::string& path)
{
  // no-op
}
#endif // _WIN32

class FileContentWriterImpl : public FileContentWriter
{
public:
  void fill(File& file) override
  {
    convertToLocalPath(file.path);
    Scanner::fillContent(file);
  }
};

bool InvocationRunner::operator()(const ScannerInvocation::MergeOptions& opts)
{
  if (globalOptions().helpFlag)
  {
    ScannerInvocation::printHelp(ScannerInvocation::Command::Merge);
    return true;
  }

  SnapshotMerger merger;

  std::vector<std::filesystem::path> scanner_directories;

  if (opts.linkMode)
  {
    scanner_directories = searchScannerDirectoriesRecursively(opts.inputs);
    std::vector<std::filesystem::path> inputs = listInputFiles(scanner_directories);

    if (inputs.empty())
    {
      std::cerr << "could not find any input file" << std::endl;
      return false;
    }
    else
    {
      std::cout << "About to merge the following files:\n";
      for (const std::filesystem::path& p : inputs)
      {
        std::cout << p << "\n";
      }
      std::cout << std::flush;
    }

    merger.setInputs(inputs);
  }
  else
  {
    for (const std::string& input : opts.inputs)
    {
      merger.addInputPath(input);
    }
  }

  // configuting output
  {
    std::filesystem::path output = "snapshot.db";

    if (opts.output.has_value())
    {
      output = *opts.output;
    }
    else if (opts.projectName.has_value())
    {
      output = *opts.projectName + ".db";
    }

    if (std::filesystem::exists(output))
    {
      std::filesystem::remove(output);
    }  

    merger.setOutputPath(output);

    std::cout << "Output file will be: " << output << std::endl;
  }

  if (opts.captureMissingFileContent || opts.linkMode)
  {
    merger.setFileContentWriter(std::make_unique<FileContentWriterImpl>());
  }

  if (opts.home.has_value())
  {
    std::cout << "Project home: " << *opts.home << std::endl;
    merger.setProjectHome(*opts.home);
  }

  if (opts.projectName.has_value())
  {
    std::cout << "Project name: " << *opts.projectName << std::endl;
    merger.setExtraProperty(PROPERTY_PROJECT_NAME, *opts.projectName);
  }

  if (opts.projectVersion.has_value())
  {
    std::cout << "Project version: " << *opts.projectVersion << std::endl;
    merger.setExtraProperty(PROPERTY_PROJECT_VERSION, *opts.projectVersion);
  }

  std::cout << "Merging..." << std::endl;

  merger.runMerge();

  if (opts.linkMode && !opts.keepSourceFiles)
  {
    std::cout << "Deleting source directories..." << std::endl;

    for (const std::filesystem::path& dir : scanner_directories)
    {
      std::filesystem::remove_all(dir);
    }

    scanner_directories.clear();
  }

  return true;
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

constexpr const char* MERGE_DESCRIPTION = R"(Description:
  Merge two or more snapshots into one.)";

void ScannerInvocation::printHelp()
{
  std::cout << "cppscanner is a clang-based command-line utility to create snapshots of C++ programs." << std::endl;
  std::cout << std::endl;
  std::cout << "Commands:" << std::endl;
  std::cout << "  run: runs the scanner to create a snapshot" << std::endl;
  std::cout << "  merge: merge two or more snapshots" << std::endl;
  std::cout << std::endl;
  std::cout << "Use the '-h' option to get more information about each command." << std::endl;
  std::cout << "Example: cppscanner run -h" << std::endl;
}

void ScannerInvocation::printHelp(Command c)
{
  if (c == Command::Run)
  {
    std::cout << "Syntax:" << std::endl;
    std::cout << "  cppscanner run --compile-commands <compile_commands.json> --output <snapshot.db> [options]" << std::endl;
    std::cout << "  cppscanner run -i <source.cpp> --output <snapshot.db> [options] [--] [compilation arguments]" << std::endl;
    std::cout << "" << std::endl;
    std::cout << RUN_OPTIONS << std::endl;
    std::cout << "" << std::endl;
    std::cout << RUN_DESCRIPTION << std::endl;
    std::cout << "" << std::endl;
    std::cout << RUN_EXAMPLES << std::endl;
  }
  else if (c == Command::Merge)
  {
    std::cout << "Syntax:" << std::endl;
    std::cout << "  cppscanner merge -o <output> input1 input2 ..." << std::endl;
    std::cout << "  cppscanner merge --link -o <output> [inputDirs]" << std::endl;
    std::cout << "" << std::endl;
    std::cout << MERGE_DESCRIPTION << std::endl;
  }
  else if (c == Command::None)
  {
    printHelp();
  }
}

ScannerInvocation::ScannerInvocation(const std::vector<std::string>& commandLine)
{
  parseCommandLine(commandLine);
  checkConsistency(m_options);
}

ScannerInvocation::ScannerInvocation()
{

}

// DO: TODO: change return type to bool instead of raising an exception
void ScannerInvocation::parseCommandLine(const std::vector<std::string>& commandLine)
{
  if (commandLine.at(0) == "run")
  {
    RunOptions result;
    parseCommandLine(result, commandLine);
    m_options.command = std::move(result);
  }
  else if (commandLine.at(0) == "merge")
  {
    MergeOptions result;
    parseCommandLine(result, commandLine);
    m_options.command = std::move(result);
  }
  else
  {
    if (!setHelpFlag(commandLine.at(0)))
    {
      std::cerr << "unknown command " << commandLine.at(0) << std::endl;
    }
  }

  if (!std::holds_alternative<std::monostate>(m_options.command))
  {
    if (commandLine.size() == 1)
    {
      // a valid command name was passed, but no argument are specified,
      // print the help.
      // note: this only works because no command currently takes zero argument,
      // in the future, we may have to force the presence of the "--help" arg.
      m_options.helpFlag = true;
    }
  }
}

bool ScannerInvocation::setHelpFlag(const std::string& arg)
{
  if (arg == "-h" || arg == "--help")
  {
    m_options.helpFlag = true;
    return true;
  }
  else
  {
    return false;
  }
}

static bool parseCliCommon(const std::vector<std::string>& commandLine, size_t i, std::optional<std::filesystem::path>& home, std::optional<std::string>& projectName, std::optional<std::string>& projectVersion)
{
  const std::string& arg = commandLine.at(i);

  if (arg == "--home")
  {
    if (++i >= commandLine.size())
      throw std::runtime_error("missing argument after --home");

    home = std::filesystem::path(commandLine.at(i++));
  }
  else if (arg == "--project-name")
  {
    if (++i >= commandLine.size())
      throw std::runtime_error("missing argument after --project-name");

    projectName = commandLine.at(i++);
  }
  else if (arg == "--project-version")
  {
    if (++i >= commandLine.size())
      throw std::runtime_error("missing argument after --project-version");

    projectVersion = commandLine.at(i++);
  }
  else
  {
    return false;
  }

  return true;
}

void ScannerInvocation::parseCommandLine(RunOptions& result, const std::vector<std::string>& commandLine)
{
  const auto& args = commandLine;

  for (size_t i(1); i < args.size();)
  {
    if (parseCliCommon(args, i, result.home, result.project_name, result.project_version))
    {
      continue;
    }

    const std::string& arg = args.at(i++);

    if (setHelpFlag(arg))
    {
      continue;
    }

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

      result.inputs.push_back(args.at(i++)); // TODO: can this be a glob expression?
    }
    else if (arg == "--output" || arg == "-o") // TODO: make common with merge command
    {
      if (i >= args.size())
        throw std::runtime_error("missing argument after --output");

      result.output = args.at(i++);
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

void ScannerInvocation::parseCommandLine(MergeOptions& result, const std::vector<std::string>& commandLine)
{
  const auto& args = commandLine;

  for (size_t i(1); i < args.size();)
  {
    if (parseCliCommon(args, i, result.home, result.projectName, result.projectVersion))
    {
      continue;
    }

    const std::string& arg = args.at(i++);

    if (setHelpFlag(arg))
    {
      continue;
    }

    if (arg == "-o" || arg == "--output") 
    {
      if (i >= args.size())
        throw std::runtime_error("missing argument after " + arg);

      result.output = args.at(i++);
    }
    else if (arg == "--capture-missing-file-content") 
    {
      result.captureMissingFileContent = true;
    }
    else if (arg == "--link") 
    {
      result.linkMode = true;
    }
    else if (arg == "--keep-source-files") 
    {
      result.keepSourceFiles = true;
    }
    else if (arg.rfind('-', 0) != 0)
    {
      result.inputs.push_back(arg);
    }
    else
    {
      throw std::runtime_error("unrecognized command line argument: " + arg);
    }
  }
}

void ScannerInvocation::parseEnv()
{
  if (std::holds_alternative<ScannerInvocation::RunOptions>(m_options.command))
  {
    parseEnv(std::get<ScannerInvocation::RunOptions>(m_options.command));
  }
  else if (std::holds_alternative<ScannerInvocation::MergeOptions>(m_options.command))
  {
    parseEnv(std::get<ScannerInvocation::MergeOptions>(m_options.command));
  }
}

static void parseEnvCommon(std::optional<std::filesystem::path>& home, std::optional<std::string>& projectName, std::optional<std::string>& projectVersion)
{
  if (!home.has_value())
  {
    std::optional<std::string> dir = readEnv(ENV_HOME_DIR);

    if (dir) {
      home = *dir;
    }
  }

  if (!projectName.has_value())
  {
    std::optional<std::string> value = readEnv(ENV_PROJECT_NAME);

    if (value) {
      projectName = *value;
    }
  }

  if (!projectVersion.has_value())
  {
    std::optional<std::string> value = readEnv(ENV_PROJECT_VERSION);

    if (value) {
      projectVersion = *value;
    }
  }
}

void ScannerInvocation::parseEnv(RunOptions& result)
{
  parseEnvCommon(result.home, result.project_name, result.project_version);

  if (!result.index_local_symbols)
  {
    result.index_local_symbols = isEnvTrue(ENV_INDEX_LOCAL_SYMBOLS);
  }
}

void ScannerInvocation::parseEnv(MergeOptions& result)
{
  parseEnvCommon(result.home, result.projectName, result.projectVersion);
}

const ScannerInvocation::Options& ScannerInvocation::options() const
{
  return m_options;
}

bool ScannerInvocation::run()
{
  InvocationRunner runner{ options(), m_errors };
  return runner.run();
}

const std::vector<std::string>& ScannerInvocation::errors() const
{
  return m_errors;
}

} // namespace cppscanner


#include "scannerinvocation.h"

#include "compilecommandsgenerator.h"

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

ScannerOptions parseCommandLine(const std::vector<std::string>& args)
{
  ScannerOptions result;

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
    else if (arg == "--threads")
    {
      if (i >= args.size())
        throw std::runtime_error("missing argument after --threads");

      result.nb_threads = std::stoi(args.at(i++));
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

  return result;
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

ScannerInvocation::ScannerInvocation(const std::vector<std::string>& command)
{
  m_options = parseCommandLine(command);
  checkConsistency(m_options);
}

const ScannerOptions& ScannerInvocation::parsedCommandLine() const
{
  return m_options;
}

const ScannerOptions& ScannerInvocation::options() const
{
  return parsedCommandLine();
}

void ScannerInvocation::run()
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
      return;
    }
  }

  Scanner scanner;

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

  scanner.setCompilationArguments(options().compilation_arguments);

  scanner.initSnapshot(options().output);

  if (options().project_name.has_value()) {
    scanner.snapshot()->setProperty("project.name", *options().project_name);
  }

  if (options().project_version.has_value()) {
    scanner.snapshot()->setProperty("project.version", *options().project_version);
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
    scanner.scanFromListOfInputs(options().inputs);
  }
}

const std::vector<std::string>& ScannerInvocation::errors() const
{
  return m_errors;
}

} // namespace cppscanner

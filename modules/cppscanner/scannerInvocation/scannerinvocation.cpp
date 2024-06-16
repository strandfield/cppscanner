
#include "scannerinvocation.h"

#include <stdexcept>

namespace cppscanner
{

namespace
{

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
    else if (arg == "--project-name")
    {
      if (i >= args.size())
        throw std::runtime_error("missing argument after --project-name");

      result.project_name = args.at(i++);
    }
    else
    {
      throw std::runtime_error("unrecognized command line argument");
    }
  }

  if (result.compile_commands.empty()) {
    throw std::runtime_error("missing input file");
  }

  if (result.output.empty()) {
    throw std::runtime_error("missing output file");
  }

  if (result.root.has_value() && !std::filesystem::is_directory(*result.root)) {
    throw std::runtime_error("Root path must be a directory");
  }

  return result;
}

} // namespace

ScannerInvocation::ScannerInvocation(const std::vector<std::string>& command)
{
  m_options = parseCommandLine(command);
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

  scanner.initSnapshot(options().output);

  if (options().project_name.has_value()) {
    scanner.snapshot()->setProperty("project.name", *options().project_name);
  }

  scanner.scan(options().compile_commands);
}

const std::vector<std::string>& ScannerInvocation::errors() const
{
  return m_errors;
}

} // namespace cppscanner

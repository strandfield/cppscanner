
#include "mergeinvocation.h"

#include "cppscanner/base/config.h"
#include "cppscanner/base/env.h"
#include "cppscanner/snapshot/merge.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace cppscanner
{

namespace
{

MergeCommandOptions parseCommandLine(const std::vector<std::string>& args)
{
  MergeCommandOptions result;

  for (size_t i(0); i < args.size();)
  {
    const std::string& arg = args.at(i++);

    if (arg == "-o" || arg == "--output") 
    {
      if (i >= args.size())
        throw std::runtime_error("missing argument after " + arg);

      result.output = args.at(i++);
    }
    else if (arg == "--home") 
    {
      if (i >= args.size())
        throw std::runtime_error("missing argument after " + arg);

      result.home = args.at(i++);
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
    else if (arg == "--project-name")
    {
      if (i >= args.size())
        throw std::runtime_error("missing argument after --project-name");

      result.projectName = args.at(i++);
    }
    else if (arg == "--project-version")
    {
      if (i >= args.size())
        throw std::runtime_error("missing argument after --project-version");

      result.projectVersion = args.at(i++);
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

  return result;
}

void checkConsistency(const MergeCommandOptions& opts)
{

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

} // namespace

class FileContentWriterImpl : public FileContentWriter
{
public:
  void fill(File& file) override
  {
    convertToLocalPath(file.path);
    Scanner::fillContent(file);
  }
};

MergeCommandInvocation::MergeCommandInvocation(const std::vector<std::string>& command)
{
  m_cli = parseCommandLine(command);
  checkConsistency(m_cli);
  m_options = m_cli;
}

void MergeCommandInvocation::printHelp()
{
  constexpr const char* MERGE_DESCRIPTION = R"(Description:
  Merge two or more snapshots into one.)";

  std::cout << "Syntax:" << std::endl;
  std::cout << "  cppscanner merge -o <output> input1 input2 ..." << std::endl;
  std::cout << "  cppscanner merge --link -o <output> [inputDirs]" << std::endl;
  std::cout << "" << std::endl;
  std::cout << MERGE_DESCRIPTION << std::endl;
}

const MergeCommandOptions& MergeCommandInvocation::parsedCommandLine() const
{
  return m_cli;
}

void MergeCommandInvocation::readEnv()
{
  if (!m_options.home.has_value())
  {
    std::optional<std::string> dir = cppscanner::readEnv(ENV_HOME_DIR);

    if (dir) {
      m_options.home = *dir;
    }
  }

  if (!m_options.projectName.has_value())
  {
    std::optional<std::string> value = cppscanner::readEnv(ENV_PROJECT_NAME);

    if (value) {
      m_options.projectName = *value;
    }
  }

  if (!m_options.projectVersion.has_value())
  {
    std::optional<std::string> value = cppscanner::readEnv(ENV_PROJECT_VERSION);

    if (value) {
      m_options.projectVersion = *value;
    }
  }
}

const MergeCommandOptions& MergeCommandInvocation::options() const
{
  return m_options;
}

bool MergeCommandInvocation::exec()
{
  std::filesystem::path output = "snapshot.db";

  if (options().output.has_value())
  {
    output = *options().output;
  }
  else if (options().projectName.has_value())
  {
    output = *options().projectName + ".db";
  }

  if (std::filesystem::exists(output))
  {
    std::filesystem::remove(output);
  }

  SnapshotMerger merger;
  merger.setOutputPath(output);

  if (options().captureMissingFileContent || options().linkMode)
  {
    merger.setFileContentWriter(std::make_unique<FileContentWriterImpl>());
  }

  std::vector<std::filesystem::path> scanner_directories;

  if (options().linkMode)
  {
    scanner_directories = searchScannerDirectoriesRecursively(options().inputs);
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
    for (const std::string& input : options().inputs)
    {
      merger.addInputPath(input);
    }
  }
  
  if (options().home.has_value())
  {
    merger.setProjectHome(*options().home);
  }

  if (options().projectName.has_value())
  {
    merger.setExtraProperty(PROPERTY_PROJECT_NAME, *options().projectName);
  }

  if (options().projectVersion.has_value())
  {
    merger.setExtraProperty(PROPERTY_PROJECT_VERSION, *options().projectVersion);
  }
  
  merger.runMerge();

  if (options().linkMode && !options().keepSourceFiles)
  {
    for (const std::filesystem::path& dir : scanner_directories)
    {
      std::filesystem::remove_all(dir);
    }

    scanner_directories.clear();
  }

  return true;
}

const std::vector<std::string>& MergeCommandInvocation::errors() const
{
  return m_errors;
}

} // namespace cppscanner

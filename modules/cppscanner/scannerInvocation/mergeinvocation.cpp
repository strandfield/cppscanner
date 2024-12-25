
#include "mergeinvocation.h"

#include "cppscanner/snapshot/merge.h"

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

const MergeCommandOptions& MergeCommandInvocation::parsedCommandLine() const
{
  return m_cli;
}

void MergeCommandInvocation::readEnv()
{
  if (!m_options.home.has_value())
  {
    const char* text = std::getenv("CPPSCANNER_HOME_DIR");

    if (text) {
      m_options.home = std::string(text);
    }
  }
}

const MergeCommandOptions& MergeCommandInvocation::options() const
{
  return m_options;
}

bool MergeCommandInvocation::exec()
{
  if (std::filesystem::exists(options().output))
  {
    std::filesystem::remove(options().output);
  }

  SnapshotMerger merger;
  merger.setOutputPath(options().output);

  if (options().captureMissingFileContent)
  {
    merger.setFileContentWriter(std::make_unique<FileContentWriterImpl>());
  }
  
  for (const std::string& input : options().inputs)
  {
    merger.addInputPath(input);
  }

  if (options().home.has_value())
  {
    merger.setProjectHome(*options().home);
  }
  
  merger.runMerge();

  return true;
}

const std::vector<std::string>& MergeCommandInvocation::errors() const
{
  return m_errors;
}

} // namespace cppscanner


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

} // namespace

MergeCommandInvocation::MergeCommandInvocation(const std::vector<std::string>& command)
{
  m_options = parseCommandLine(command);
  checkConsistency(m_options);
}

const MergeCommandOptions& MergeCommandInvocation::parsedCommandLine() const
{
  return m_options;
}

const MergeCommandOptions& MergeCommandInvocation::options() const
{
  return parsedCommandLine();
}

bool MergeCommandInvocation::exec()
{
  
  if (std::filesystem::exists(options().output))
  {
    std::filesystem::remove(options().output);
  }

  SnapshotMerger merger;
  merger.setOutputPath(options().output);
  
  for (const std::string& input : options().inputs)
  {
    merger.addInputPath(input);
  }

  merger.runMerge();

  return true;
}

const std::vector<std::string>& MergeCommandInvocation::errors() const
{
  return m_errors;
}

} // namespace cppscanner

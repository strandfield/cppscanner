
#include "cmakeinvocation.h"

#include <llvm/Support/Process.h>

#include <fstream>
#include <iostream>
#include <stdexcept>

namespace cppscanner
{

namespace
{

CMakeCommandOptions parseCommandLine(const std::vector<std::string>& args)
{
  CMakeCommandOptions result;

  result.arguments.assign(args.begin(), args.end());

  CMakeCommandOptions::InputPaths paths;
  bool bsMode = false;

  for (size_t i(0); i < args.size();)
  {
    const std::string& arg = args.at(i++);

    if (arg == "-B" || arg == "-S") 
    {
      bsMode = true;

      if (i >= args.size())
        throw std::runtime_error("missing argument after " + arg);

      if (arg == "-B") {
        paths.pathToBuild = args.at(i++);
      } else {
        paths.pathToSource = std::filesystem::path(args.at(i++));
      }
    }
  }

  if (bsMode) {
    result.inputDirectories = paths;
  } else {
    result.inputDirectories = std::filesystem::path(args.back());
  }

  return result;
}

void checkConsistency(const CMakeCommandOptions& opts)
{
  if (std::holds_alternative<CMakeCommandOptions::InputPaths>(opts.inputDirectories))
  {
    const auto& paths = std::get<CMakeCommandOptions::InputPaths>(opts.inputDirectories);
    if (paths.pathToBuild.empty())
    {
      throw std::runtime_error("missing build directory");
    }
  }
}

} // namespace

CMakeCommandInvocation::CMakeCommandInvocation(const std::vector<std::string>& command)
{
  m_options = parseCommandLine(command);
  checkConsistency(m_options);
}

const CMakeCommandOptions& CMakeCommandInvocation::parsedCommandLine() const
{
  return m_options;
}

const CMakeCommandOptions& CMakeCommandInvocation::options() const
{
  return parsedCommandLine();
}

bool CMakeCommandInvocation::exec()
{
  if (std::holds_alternative<CMakeCommandOptions::InputPaths>(options().inputDirectories))
  {
    const auto& paths = std::get<CMakeCommandOptions::InputPaths>(options().inputDirectories);
    assert(!paths.pathToBuild.empty());
    writeQueryFile(paths.pathToBuild);
  }
  else
  {
    const std::filesystem::path& build_or_source = std::get<std::filesystem::path>(options().inputDirectories);

    if (std::filesystem::exists(build_or_source / "CMakeLists.txt"))
    {
      // if a source directory was specified, then the build directory is the
      // current working dir
      writeQueryFile(std::filesystem::current_path());
    }
    else if (std::filesystem::exists(build_or_source / "CMakeCache.txt"))
    {
      writeQueryFile(build_or_source);
    }
    else
    {
      std::cerr << "could not deduce if " << build_or_source << " is a build or source directory" << std::endl;
      return false;
    }
  }

  const std::string program = llvm::sys::findProgramByName("cmake").get();

  std::vector<llvm::StringRef> arguments;
  arguments.push_back(program);
  arguments.insert(arguments.end(), options().arguments.begin(), options().arguments.end());

  const int r = llvm::sys::ExecuteAndWait(program, arguments);

  if (r < 0)
  {
    std::cerr << "an error occured while invoking cmake" << std::endl;
  }
  else if (r > 0)
  {
    std::cerr << "cmake returned a non-zero exit code (" << r << ")" << std::endl;
  }

  return r == 0;
}

void CMakeCommandInvocation::writeQueryFile(const std::filesystem::path& buildDir)
{
  assert(!buildDir.empty());
  const std::filesystem::path dir = buildDir / ".cmake" / "api" / "v1" / "query" / "client-cppscanner";
  std::filesystem::create_directories(dir);

  std::ofstream file{ dir / "query.json", std::ios::out | std::ios::trunc };

  file << R"({
  "requests": [
    {"kind":"cache","version":2},
    {"kind":"codemodel","version":2},
    {"kind":"toolchains","version":1},
    {"kind":"cmakeFiles","version":1}
  ]
})";
}

const std::vector<std::string>& CMakeCommandInvocation::errors() const
{
  return m_errors;
}

} // namespace cppscanner

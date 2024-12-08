// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

// https://cmake.org/cmake/help/latest/manual/cmake.1.html

#ifndef CPPSCANNER_CMAKEINVOCATION_H
#define CPPSCANNER_CMAKEINVOCATION_H

#include "cppscanner/indexer/scanner.h"

#include <filesystem>
#include <optional>
#include <variant>

namespace cppscanner
{

struct CMakeCommandOptions
{
  std::vector<std::string> arguments; 

  struct InputPaths
  {
    std::filesystem::path pathToBuild;
    std::optional<std::filesystem::path> pathToSource;
  };

  std::variant<std::filesystem::path, InputPaths> inputDirectories;
};

/**
 * \brief represents a command-line invocation of the scanner cmake command
 */
class CMakeCommandInvocation
{
private:
  CMakeCommandOptions m_options;
  std::vector<std::string> m_errors;

public:
  explicit CMakeCommandInvocation(const std::vector<std::string>& command);

  const CMakeCommandOptions& parsedCommandLine() const;
  const CMakeCommandOptions& options() const;

  bool exec();

  const std::vector<std::string>& errors() const;

private:
  void writeQueryFile(const std::filesystem::path& buildDir);
};

} // namespace cppscanner

#endif // CPPSCANNER_CMAKEINVOCATION_H

// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_MERGEINVOCATION_H
#define CPPSCANNER_MERGEINVOCATION_H

#include "cppscanner/indexer/scanner.h"

#include <filesystem>
#include <optional>
#include <variant>

namespace cppscanner
{

struct MergeCommandOptions
{
  std::vector<std::string> inputs; 
  std::filesystem::path output;
  std::optional<std::filesystem::path> home;
};

/**
 * \brief represents a command-line invocation of the scanner's merge command
 */
class MergeCommandInvocation
{
private:
  MergeCommandOptions m_cli;
  MergeCommandOptions m_options;
  std::vector<std::string> m_errors;

public:
  explicit MergeCommandInvocation(const std::vector<std::string>& command);

  const MergeCommandOptions& parsedCommandLine() const;

  void readEnv();

  const MergeCommandOptions& options() const;

  bool exec();

  const std::vector<std::string>& errors() const;
};

} // namespace cppscanner

#endif // CPPSCANNER_MERGEINVOCATION_H

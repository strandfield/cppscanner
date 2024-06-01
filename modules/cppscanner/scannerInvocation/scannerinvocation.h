// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_SCANNERINVOCATION_H
#define CPPSCANNER_SCANNERINVOCATION_H

#include "cppscanner/indexer/scanner.h"

#include <filesystem>
#include <optional>

namespace cppscanner
{

struct ScannerOptions
{
  std::filesystem::path compile_commands;
  std::filesystem::path output;
  std::optional<std::filesystem::path> home;
  bool overwrite = false;
  bool no_home = false;
  bool index_local_symbols = false;
  std::vector<std::string> filters;
  std::optional<std::string> project_name;
};

class ScannerInvocation
{
private:
  ScannerOptions m_options;
  std::vector<std::string> m_errors;

public:
  explicit ScannerInvocation(const std::vector<std::string>& command);

  const ScannerOptions& parsedCommandLine() const;
  const ScannerOptions& options() const;

  void run();

  const std::vector<std::string>& errors() const;
};

} // namespace cppscanner

#endif // CPPSCANNER_SCANNERINVOCATION_H

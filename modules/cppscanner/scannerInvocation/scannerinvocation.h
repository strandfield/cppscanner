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

/**
 * \brief the command-line options of the scanner
 */
struct ScannerOptions
{
  std::vector<std::filesystem::path> inputs;
  std::optional<std::filesystem::path> compile_commands;
  std::optional<std::filesystem::path> cmakeBuildDirectory;
  std::optional<std::string> cmakeConfig;
  std::vector<std::string> cmakeTargets;
  std::filesystem::path output;
  std::optional<std::filesystem::path> home;
  std::optional<std::filesystem::path> root;
  bool overwrite = false;
  bool index_external_files = false;
  bool index_local_symbols = false;
  bool ignore_file_content = false;
  std::optional<int> nb_threads;
  std::vector<std::string> filters;
  std::vector<std::string> translation_unit_filters;
  std::optional<std::string> project_name;
  std::optional<std::string> project_version;
  std::vector<std::string> compilation_arguments; // arguments passed after --
};

/**
 * \brief represents a command-line invocation of the scanner
 */
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

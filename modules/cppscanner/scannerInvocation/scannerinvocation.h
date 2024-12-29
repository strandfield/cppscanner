// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_SCANNERINVOCATION_H
#define CPPSCANNER_SCANNERINVOCATION_H

#include "cppscanner/indexer/scanner.h"

#include <filesystem>
#include <optional>
#include <variant>

namespace cppscanner
{

/**
 * \brief represents a command-line invocation of the scanner
 */
class ScannerInvocation
{
public:
  ScannerInvocation();
  explicit ScannerInvocation(const std::vector<std::string>& commandLine);

  enum class Command
  {
    None,
    Run,
  };

  static void printHelp();
  static void printHelp(Command c);

  /**
   * \brief options for the "run" command
   */
  struct RunOptions
  {
    std::vector<std::filesystem::path> inputs;
    std::optional<std::filesystem::path> compile_commands;
    std::filesystem::path output;
    std::optional<std::filesystem::path> home;
    std::optional<std::filesystem::path> root;
    bool overwrite = false;
    bool index_external_files = false;
    bool index_local_symbols = false;
    bool ignore_file_content = false;
    bool remap_file_ids = false;
    std::optional<int> nb_threads;
    std::vector<std::string> filters;
    std::vector<std::string> translation_unit_filters;
    std::optional<std::string> project_name;
    std::optional<std::string> project_version;
    std::vector<std::string> compilation_arguments; // arguments passed after --
  };

  struct Options
  {
    bool helpFlag = false;
    std::variant<std::monostate, RunOptions> command;
  };

  void parseCommandLine(const std::vector<std::string>& commandLine);
  void parseEnv();

  const Options& options() const;

  bool run();

  const std::vector<std::string>& errors() const;

private:
  void parseCommandLine(RunOptions& result, const std::vector<std::string>& commandLine);
  void parseEnv(RunOptions& result);

private:
  Options m_options;
  std::vector<std::string> m_errors;
};

} // namespace cppscanner

#endif // CPPSCANNER_SCANNERINVOCATION_H

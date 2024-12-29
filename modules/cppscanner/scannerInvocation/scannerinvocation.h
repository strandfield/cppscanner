// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_SCANNERINVOCATION_H
#define CPPSCANNER_SCANNERINVOCATION_H

#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace cppscanner
{

/**
 * \brief represents a command-line invocation of cppscanner
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
    Merge,
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
    std::optional<std::filesystem::path> output;
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

  /**
   * \brief options for the "merge" command
   */
  struct MergeOptions
  {
    std::vector<std::string> inputs; 
    std::optional<std::filesystem::path> output;
    std::optional<std::filesystem::path> home;
    bool captureMissingFileContent = false;
    bool linkMode = false;
    bool keepSourceFiles = false;
    std::optional<std::string> projectName;
    std::optional<std::string> projectVersion;
  };

  struct Options
  {
    bool helpFlag = false;
    std::variant<std::monostate, RunOptions, MergeOptions> command;
  };

  bool parseCommandLine(const std::vector<std::string>& commandLine);
  void parseEnv();

  const Options& options() const;

  bool run();

  const std::vector<std::string>& errors() const;

private:
  bool setHelpFlag(const std::string& arg);
  void parseCommandLine(RunOptions& result, const std::vector<std::string>& commandLine);
  void parseCommandLine(MergeOptions& result, const std::vector<std::string>& commandLine);

  void parseEnv(RunOptions& result);
  void parseEnv(MergeOptions& result);

private:
  Options m_options;
  std::vector<std::string> m_errors;
};

} // namespace cppscanner

#endif // CPPSCANNER_SCANNERINVOCATION_H

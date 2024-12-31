// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_SCANNER_H
#define CPPSCANNER_SCANNER_H

#include "snapshotcreator.h"

#include <filesystem>
#include <string>
#include <vector>

namespace clang
{
class FileManager;
} // namespace clang

namespace cppscanner
{

class FileIndexingArbiter;
class TranslationUnitIndex;

struct ScannerData;

struct CompileCommand
{
  std::string fileName;
  std::vector<std::string> commandLine;
};

struct ScannerCompileCommand;

/**
 * \brief top level class for indexing a C++ project and creating a snapshot 
 * 
 * The Scanner class uses the Indexer class for producing a TranslationUnitIndex
 * for each translation unit in the project and then aggregates the results
 * in a single database file.
 * 
 * Warning: the initSnapshot() must be called before scan().
 */
class Scanner
{
public:
  Scanner();
  Scanner(const Scanner&) = delete;
  ~Scanner();

  void setOutputPath(const std::filesystem::path& p);
  void setHomeDir(const std::filesystem::path& p);
  void setRootDir(const std::filesystem::path& p);

  void setIndexExternalFiles(bool on = true);
  void setIndexLocalSymbols(bool on = true);

  void setFilters(const std::vector<std::string>& filters);
  void setTranslationUnitFilters(const std::vector<std::string>& filters);

  void setNumberOfParsingThread(size_t n);

  void setCaptureFileContent(bool on = true);
  void setRemapFileIds(bool on);

  void setExtraProperty(const std::string& name, const std::string& value);

  void scanFromCompileCommands(const std::filesystem::path& compileCommandsPath);
  void scanFromListOfInputs(const std::vector<std::filesystem::path>& inputs, const std::vector<std::string>& compileArgs);
  void scan(const std::vector<CompileCommand>& compileCommands);

  static void fillContent(File& file);

protected:
  void initSnapshot();
  SnapshotCreator* snapshotCreator() const;

  bool passTranslationUnitFilters(const std::string& filename) const;
  void scanSingleThreaded();
  void scanMultiThreaded();
  void runScanSingleOrMultiThreaded();
  void processCommands(const std::vector<ScannerCompileCommand>& commands, FileIndexingArbiter& arbiter, clang::FileManager& fileManager);

private:
  std::unique_ptr<SnapshotCreator> m_snapshot_creator;
  std::unique_ptr<ScannerData> d;
};

} // namespace cppscanner

#endif // CPPSCANNER_SCANNER_H

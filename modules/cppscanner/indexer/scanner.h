// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_SCANNER_H
#define CPPSCANNER_SCANNER_H

#include "cppscanner/snapshot/snapshotwriter.h"

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

struct ScannerCompileCommand
{
  std::string fileName;
  std::vector<std::string> commandLine;
};

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

  void setHomeDir(const std::filesystem::path& p);
  void setRootDir(const std::filesystem::path& p);

  void setIndexExternalFiles(bool on = true);
  void setIndexLocalSymbols(bool on = true);

  void setFilters(const std::vector<std::string>& filters);
  void setTranslationUnitFilters(const std::vector<std::string>& filters);

  void setNumberOfParsingThread(size_t n);

  void setCaptureFileContent(bool on = true);
  void setRemapFileIds(bool on);

  void setCompilationArguments(const std::vector<std::string>& args);

  void initSnapshot(const std::filesystem::path& p);
  SnapshotWriter* snapshot() const;

  void scanFromCompileCommands(const std::filesystem::path& compileCommandsPath);
  void scanFromListOfInputs(const std::vector<std::filesystem::path>& inputs);
  void scan(const std::vector<ScannerCompileCommand>& compileCommands);

  static void fillContent(File& file);

protected:
  bool passTranslationUnitFilters(const std::string& filename) const;
  void scanSingleThreaded();
  void scanMultiThreaded();
  void runScanSingleOrMultiThreaded();
  void processCommands(const std::vector<ScannerCompileCommand>& commands, FileIndexingArbiter& arbiter, clang::FileManager& fileManager);
  void assimilate(TranslationUnitIndex tuIndex);
  bool fileAlreadyIndexed(FileID f) const;
  void setFileIndexed(FileID f);

private:
  std::unique_ptr<SnapshotWriter> m_snapshot;
  std::unique_ptr<ScannerData> d;
};

} // namespace cppscanner

#endif // CPPSCANNER_SCANNER_H

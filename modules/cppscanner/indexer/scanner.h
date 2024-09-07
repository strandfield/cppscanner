// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_SCANNER_H
#define CPPSCANNER_SCANNER_H

#include "snapshotwriter.h"

#include <filesystem>

namespace cppscanner
{

class TranslationUnitIndex;

struct ScannerData;

/**
 * \brief top level class for indexing a C++ project and creating a snapshot 
 * 
 * The initSnapshot() must be called before scan().
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

  void initSnapshot(const std::filesystem::path& p);
  SnapshotWriter* snapshot() const;

  void scan(const std::filesystem::path& compileCommandsPath);

protected:
  void assimilate(TranslationUnitIndex tuIndex);
  bool fileAlreadyIndexed(FileID f) const;
  void setFileIndexed(FileID f);

private:
  std::unique_ptr<SnapshotWriter> m_snapshot;
  std::unique_ptr<ScannerData> d;
};

} // namespace cppscanner

#endif // CPPSCANNER_SCANNER_H

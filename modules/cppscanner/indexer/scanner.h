// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_SCANNER_H
#define CPPSCANNER_SCANNER_H

#include "snapshot.h"

#include <filesystem>

namespace cppscanner
{

class TranslationUnitIndex;

struct ScannerData;

/**
 * \brief top level class for creating a snapshot 
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
  Snapshot* snapshot() const;

  void scan(const std::filesystem::path& compileCommandsPath);

protected:
  void assimilate(TranslationUnitIndex tuIndex);
  bool fileAlreadyIndexed(FileID f) const;
  void setFileIndexed(FileID f);

private:
  std::unique_ptr<Snapshot> m_snapshot;
  std::unique_ptr<ScannerData> d;
};

} // namespace cppscanner

#endif // CPPSCANNER_SCANNER_H

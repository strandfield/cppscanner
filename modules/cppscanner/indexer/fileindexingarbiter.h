// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_FILEINDEXINGARBITER_H
#define CPPSCANNER_FILEINDEXINGARBITER_H

#include "cppscanner/index/fileid.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace cppscanner
{

class FileIdentificator;

class FileIndexingArbiter
{
private:
  FileIdentificator& m_fileIdentificator;

public:
  explicit FileIndexingArbiter(FileIdentificator& fIdentificator);
  virtual ~FileIndexingArbiter();

  FileIdentificator& fileIdentificator() const;

  virtual bool shouldIndex(FileID file, void* context);

  static std::unique_ptr<FileIndexingArbiter> createCompositeArbiter(std::vector<std::unique_ptr<FileIndexingArbiter>> arbiters);
  static std::unique_ptr<FileIndexingArbiter> createThreadSafeArbiter(std::unique_ptr<FileIndexingArbiter> arbiter);
};

inline FileIdentificator& FileIndexingArbiter::fileIdentificator() const
{
  return m_fileIdentificator;
}

class IndexOnceFileIndexingArbiter : public FileIndexingArbiter
{
private:
  std::map<FileID, void*> m_indexers;

public:
  explicit IndexOnceFileIndexingArbiter(FileIdentificator& fIdentificator);

  bool shouldIndex(FileID file, void* idxr) final;
};

class IndexDirectoryFileIndexingArbiter : public FileIndexingArbiter
{
private:
  std::string m_dir_path;

public:
  explicit IndexDirectoryFileIndexingArbiter(FileIdentificator& fIdentificator, const std::string& dir);

  bool shouldIndex(FileID file, void* idxr) final;
};

bool filename_match(const std::string& filePath, const std::string& fileName);

class IndexFilesMatchingPatternIndexingArbiter : public FileIndexingArbiter
{
private:
  std::vector<std::string> m_patterns;

public:
  explicit IndexFilesMatchingPatternIndexingArbiter(FileIdentificator& fIdentificator, const std::vector<std::string>& patterns);

  bool shouldIndex(FileID file, void* idxr) final;
};

} // namespace cppscanner

#endif // CPPSCANNER_FILEINDEXINGARBITER_H

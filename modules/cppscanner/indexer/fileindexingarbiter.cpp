// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#include "fileindexingarbiter.h"

#include "fileidentificator.h"
#include "glob.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <mutex>

namespace cppscanner
{

class CompositeFileIndexingArbiter : public FileIndexingArbiter
{
private:
  using ArbiterPtr = std::unique_ptr<FileIndexingArbiter>;
  std::vector<ArbiterPtr> m_arbiters;

public:
  explicit CompositeFileIndexingArbiter(std::vector<std::unique_ptr<FileIndexingArbiter>> arbiters) :
    FileIndexingArbiter(arbiters.front()->fileIdentificator()),
    m_arbiters(std::move(arbiters))
  {

  }

  bool shouldIndex(FileID file, void* idxr) final
  {
    return std::all_of(m_arbiters.begin(), m_arbiters.end(), [file, idxr](const ArbiterPtr& arbiter) {
      return arbiter->shouldIndex(file, idxr);
      });
  }
};


class ThreadSafeFileIndexingArbiter : public FileIndexingArbiter
{
private:
  std::unique_ptr<FileIndexingArbiter> m_arbiter;
  std::mutex m_mutex;

public:
  explicit ThreadSafeFileIndexingArbiter(std::unique_ptr<FileIndexingArbiter> arbiter);

  bool shouldIndex(FileID file, void* context) final;
};

ThreadSafeFileIndexingArbiter::ThreadSafeFileIndexingArbiter(std::unique_ptr<FileIndexingArbiter> arbiter) :
  FileIndexingArbiter(arbiter->fileIdentificator()),
  m_arbiter(std::move(arbiter))
{

}

bool ThreadSafeFileIndexingArbiter::shouldIndex(FileID file, void* context)
{
  std::lock_guard lock{ m_mutex };
  return m_arbiter->shouldIndex(file, context);
}

FileIndexingArbiter::FileIndexingArbiter(FileIdentificator& fIdentificator) :
  m_fileIdentificator(fIdentificator)
{

}

FileIndexingArbiter::~FileIndexingArbiter()
{

}

bool FileIndexingArbiter::shouldIndex(FileID /* file */, void* /* context */)
{
  return true;
}

std::unique_ptr<FileIndexingArbiter> FileIndexingArbiter::createCompositeArbiter(std::vector<std::unique_ptr<FileIndexingArbiter>> arbiters)
{
  assert(!arbiters.empty());
  // TODO: verify that all arbiters use the same file ids

  if (arbiters.size() == 1) {
    return std::unique_ptr<FileIndexingArbiter>(arbiters.front().release());
  } else {
    return std::make_unique<CompositeFileIndexingArbiter>(std::move(arbiters));
  }
}

std::unique_ptr<FileIndexingArbiter> FileIndexingArbiter::createThreadSafeArbiter(std::unique_ptr<FileIndexingArbiter> arbiter)
{
  return std::make_unique<ThreadSafeFileIndexingArbiter>(std::move(arbiter));
}

IndexOnceFileIndexingArbiter::IndexOnceFileIndexingArbiter(FileIdentificator& fIdentificator) :
  FileIndexingArbiter(fIdentificator)
{

}

bool IndexOnceFileIndexingArbiter::shouldIndex(FileID file, void* idxr)
{
  if (!file) {
    return false;
  }

  auto it = m_indexers.find(file);

  if (it != m_indexers.end()) {
    return it->second == idxr;
  }

  m_indexers[file] = idxr;
  return true;
}

IndexDirectoryFileIndexingArbiter::IndexDirectoryFileIndexingArbiter(FileIdentificator& fIdentificator, const std::string& dir) : 
  FileIndexingArbiter(fIdentificator),
  m_dir_path(std::filesystem::absolute(dir).generic_u8string())
{

}

bool IndexDirectoryFileIndexingArbiter::shouldIndex(FileID file, void* idxr)
{
  (void)idxr;

  std::string path = fileIdentificator().getFile(file);
  path = std::filesystem::absolute(path).generic_u8string();

  return path.size() > m_dir_path.size() &&
    path.at(m_dir_path.size()) == '/' &&
    std::strncmp(path.c_str(), m_dir_path.c_str(), m_dir_path.size()) == 0;
}

IndexFilesMatchingPatternIndexingArbiter::IndexFilesMatchingPatternIndexingArbiter(FileIdentificator& fIdentificator, const std::vector<std::string>& patterns) :
  FileIndexingArbiter(fIdentificator),
  m_patterns(patterns)
{

}

bool filename_match(const std::string& filePath, const std::string& fileName)
{
  return filePath.size() >= fileName.size() &&
    filePath.find(fileName, filePath.size() - fileName.size()) != std::string::npos;
}

bool IndexFilesMatchingPatternIndexingArbiter::shouldIndex(FileID file, void* idxr)
{
  (void)idxr;

  std::string path = fileIdentificator().getFile(file);

  return std::any_of(m_patterns.begin(), m_patterns.end(), [&path](const std::string& e) {
    return is_glob_pattern(e) ? glob_match(path, e) : filename_match(path, e);
    });
}

} // namespace cppscanner

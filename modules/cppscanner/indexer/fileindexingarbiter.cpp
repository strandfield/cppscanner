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
    FileIdentificator* fid = &fileIdentificator();
    bool ok = std::all_of(m_arbiters.begin(), m_arbiters.end(), [fid](const ArbiterPtr& arbiter) {
      return &arbiter->fileIdentificator() == fid;
      });
    if (!ok) {
      throw std::runtime_error("not all arbiters use the same fileidentificator");
    }
  }

  bool shouldIndex(FileID file, const Indexer* idxr) final
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

  bool shouldIndex(FileID file, const Indexer* context) final;
};

ThreadSafeFileIndexingArbiter::ThreadSafeFileIndexingArbiter(std::unique_ptr<FileIndexingArbiter> arbiter) :
  FileIndexingArbiter(arbiter->fileIdentificator()),
  m_arbiter(std::move(arbiter))
{

}

bool ThreadSafeFileIndexingArbiter::shouldIndex(FileID file, const Indexer* context)
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

/**
 * \brief returns whether a file should be indexed
 * \param file     the file id
 * \param indexer  pointer to the indexer (optional)
 * 
 * If \a indexer is nullptr, the function returns whether the file should be indexed.
 * Otherwise, it returns whether the file should be indexed by the the given \a indexer.
 * 
 * \sa IndexOnceFileIndexingArbiter
 */
bool FileIndexingArbiter::shouldIndex(FileID /* file */, const Indexer* /* indexer */)
{
  return true;
}

/**
 * \brief creates a file indexing arbiter from a list of arbiters
 * \param arbiters  a list of file indexing arbiters
 * 
 * The resulting arbiter shouldIndex() method returns true when shouldIndex()
 * returns true for all the \a arbiters.
 */
std::unique_ptr<FileIndexingArbiter> FileIndexingArbiter::createCompositeArbiter(std::vector<std::unique_ptr<FileIndexingArbiter>> arbiters)
{
  assert(!arbiters.empty());

  if (arbiters.size() == 1) {
    return std::unique_ptr<FileIndexingArbiter>(arbiters.front().release());
  } else {
    return std::make_unique<CompositeFileIndexingArbiter>(std::move(arbiters));
  }
}

/**
 * \brief creates a thread-safe file indexing arbiter from an existing one
 * \param arbiter  a non-null arbiter
 * 
 * This creates an arbiter that protects access to \a arbiter with a mutex.
 * Using a thread-safe arbiter is required when using an arbiter that maintains 
 * state (such as IndexOnceFileIndexingArbiter) in multiple threads.
 */
std::unique_ptr<FileIndexingArbiter> FileIndexingArbiter::createThreadSafeArbiter(std::unique_ptr<FileIndexingArbiter> arbiter)
{
  return std::make_unique<ThreadSafeFileIndexingArbiter>(std::move(arbiter));
}

IndexOnceFileIndexingArbiter::IndexOnceFileIndexingArbiter(FileIdentificator& fIdentificator) :
  FileIndexingArbiter(fIdentificator)
{

}

/**
 * \brief returns whether the file should be indexed by a given indexer
 * 
 * The first time this function is called with a non-null indexer (for a given
 * file), it assigns the file to the indexer and will subsequently return true
 * on that file only for that indexer.
 */
bool IndexOnceFileIndexingArbiter::shouldIndex(FileID file, const Indexer* idxr)
{
  if (!file) {
    return false;
  }

  if (!idxr) {
    return true;
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

bool IndexDirectoryFileIndexingArbiter::shouldIndex(FileID file, const Indexer* idxr)
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

bool IndexFilesMatchingPatternIndexingArbiter::shouldIndex(FileID file, const Indexer* idxr)
{
  (void)idxr;

  std::string path = fileIdentificator().getFile(file);

  return std::any_of(m_patterns.begin(), m_patterns.end(), [&path](const std::string& e) {
    return is_glob_pattern(e) ? glob_match(path, e) : filename_match(path, e);
    });
}

} // namespace cppscanner

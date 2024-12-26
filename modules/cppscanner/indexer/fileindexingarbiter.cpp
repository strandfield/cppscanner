// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#include "fileindexingarbiter.h"

#include "fileidentificator.h"

#include "cppscanner/base/glob.h"

#include <algorithm>
#include <cassert>
#include <cstring>
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

  bool shouldIndex(FileID file, const TranslationUnitIndex* tu) final
  {
    return std::all_of(m_arbiters.begin(), m_arbiters.end(), [file, tu](const ArbiterPtr& arbiter) {
      return arbiter->shouldIndex(file, tu);
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

  bool shouldIndex(FileID file, const TranslationUnitIndex* context) final;
};

ThreadSafeFileIndexingArbiter::ThreadSafeFileIndexingArbiter(std::unique_ptr<FileIndexingArbiter> arbiter) :
  FileIndexingArbiter(arbiter->fileIdentificator()),
  m_arbiter(std::move(arbiter))
{

}

bool ThreadSafeFileIndexingArbiter::shouldIndex(FileID file, const TranslationUnitIndex* context)
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
 * \param file  the file id
 * \param tu    pointer to the translation unit (optional)
 * 
 * If \a tu is nullptr, the function returns whether the file should be indexed.
 * Otherwise, it returns whether the file should be indexed within the translation unit.
 * 
 * \sa IndexOnceFileIndexingArbiter
 */
bool FileIndexingArbiter::shouldIndex(FileID /* file */, const TranslationUnitIndex* /* tu */)
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
 * \brief returns whether the file should be indexed in a given translation unit
 * 
 * The first time this function is called with a non-null translation unit (for a given
 * file), it assigns the file to the translation unit and will subsequently return true
 * on that file only for that translation unit.
 */
bool IndexOnceFileIndexingArbiter::shouldIndex(FileID file, const TranslationUnitIndex* tu)
{
  if (!file) {
    return false;
  }

  if (!tu) {
    return true;
  }

  auto it = m_translation_units.find(file);

  if (it != m_translation_units.end()) {
    return it->second == tu;
  }

  m_translation_units[file] = tu;
  return true;
}

IndexDirectoryFileIndexingArbiter::IndexDirectoryFileIndexingArbiter(FileIdentificator& fIdentificator, const std::string& dir) : 
  FileIndexingArbiter(fIdentificator),
  m_dir_path(std::filesystem::absolute(dir).generic_u8string())
{

}

bool IndexDirectoryFileIndexingArbiter::shouldIndex(FileID file, const TranslationUnitIndex* tu)
{
  (void)tu;

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

bool IndexFilesMatchingPatternIndexingArbiter::shouldIndex(FileID file, const TranslationUnitIndex* tu)
{
  (void)tu;

  std::string path = fileIdentificator().getFile(file);

  return std::any_of(m_patterns.begin(), m_patterns.end(), [&path](const std::string& e) {
    return is_glob_pattern(e) ? glob_match(path, e) : filename_match(path, e);
    });
}

std::unique_ptr<FileIndexingArbiter> createIndexingArbiter(FileIdentificator& fileIdentificator, const CreateIndexingArbiterOptions& opts)
{
  std::vector<std::unique_ptr<FileIndexingArbiter>> arbiters;

  arbiters.push_back(std::make_unique<IndexOnceFileIndexingArbiter>(fileIdentificator));

  if (opts.indexExternalFiles) {
    if (!opts.rootDirectory.empty()) {
      arbiters.push_back(std::make_unique<IndexDirectoryFileIndexingArbiter>(fileIdentificator, opts.rootDirectory));
    }
  } else {
    arbiters.push_back(std::make_unique<IndexDirectoryFileIndexingArbiter>(fileIdentificator, opts.homeDirectory));
  }

  if (!opts.filters.empty()) {
    arbiters.push_back(std::make_unique<IndexFilesMatchingPatternIndexingArbiter>(fileIdentificator, opts.filters));
  }

  return FileIndexingArbiter::createCompositeArbiter(std::move(arbiters));
}

} // namespace cppscanner

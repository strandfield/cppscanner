// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_INDEXINGRESULTQUEUE_H
#define CPPSCANNER_INDEXINGRESULTQUEUE_H

#include "translationunitindex.h"

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

namespace cppscanner
{

class IndexingResultQueue
{
private:
  std::queue<TranslationUnitIndex> m_indexingResults;

  struct Synchronization {
    std::mutex mutex;
    std::condition_variable cv;
  };

  mutable Synchronization m_sync;

public:
  IndexingResultQueue() = default;
  IndexingResultQueue(const IndexingResultQueue&) = delete;

  void write(TranslationUnitIndex index)
  {
    {
      std::lock_guard lock{ m_sync.mutex };
      m_indexingResults.push(std::move(index));
    }

    m_sync.cv.notify_one();
  }

  std::optional<TranslationUnitIndex> tryRead(std::chrono::milliseconds timeout)
  {
    std::unique_lock lock{ m_sync.mutex };

    m_sync.cv.wait_for(lock, timeout, [&]() {
      return !m_indexingResults.empty();
      });

    return unsafeRead();
  }

  std::optional<TranslationUnitIndex> readSync()
  {
    std::unique_lock lock{ m_sync.mutex };
    return unsafeRead();
  }

protected:
  std::optional<TranslationUnitIndex> unsafeRead()
  {
    if (m_indexingResults.empty()) {
      return std::nullopt;
    } else {
      TranslationUnitIndex idx{ std::move(m_indexingResults.front()) };
      m_indexingResults.pop();
      return idx;
    }
  }
};

} // namespace cppscanner

#endif // CPPSCANNER_INDEXINGRESULTQUEUE_H

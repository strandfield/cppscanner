// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_WORKQUEUE_H
#define CPPSCANNER_WORKQUEUE_H

#include <mutex>
#include <optional>
#include <queue>
#include <vector>

#include <iostream>

namespace cppscanner
{

class WorkQueue
{
public:
  WorkQueue() = default;
  WorkQueue(const WorkQueue&) = delete;

  struct ToolInvocation
  {
    std::string filename;
    std::vector<std::string> command;
  };

  explicit WorkQueue(const std::vector<ToolInvocation>& tasks)
  {
    for (const ToolInvocation& item : tasks)
    {
      m_queue.push(item);
    }
  }

  void push(const std::vector<ToolInvocation>& tasks)
  {
    std::lock_guard lock{ m_sync.mutex };

    for (const ToolInvocation& item : tasks)
    {
      m_queue.push(item);
    }
  }

  std::optional<ToolInvocation> next()
  {
    std::unique_lock lock{ m_sync.mutex };

    if (m_queue.empty()) {
      return std::nullopt;
    } else {
      ToolInvocation item{ std::move(m_queue.front()) };
      m_queue.pop();

      // not clean...
      std::cout << item.filename << std::endl;

      return item;
    }
  }

private:
  std::queue<ToolInvocation> m_queue;

  struct Synchronization {
    std::mutex mutex;
  };

  mutable Synchronization m_sync;
};

} // namespace cppscanner

#endif // CPPSCANNER_WORKQUEUE_H

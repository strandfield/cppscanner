// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_THREADSAFEFILEIDENTIFICATOR_H
#define CPPSCANNER_THREADSAFEFILEIDENTIFICATOR_H

#include "basicfileidentificator.h"

#include <mutex>

namespace cppscanner
{

class ThreadSafeFileIdentificator : public FileIdentificator
{
private:
  BasicFileIdentificator m_files;
  mutable std::mutex m_mutex;

public:
  explicit ThreadSafeFileIdentificator(std::map<std::string, FileID> files = {});

  FileID getIdentification(const std::string& file) final;
  std::vector<std::string> getFiles() const final;
};

} // namespace cppscanner

#endif // CPPSCANNER_THREADSAFEFILEIDENTIFICATOR_H

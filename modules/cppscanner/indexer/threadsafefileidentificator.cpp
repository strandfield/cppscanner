// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#include "threadsafefileidentificator.h"

namespace cppscanner
{

ThreadSafeFileIdentificator::ThreadSafeFileIdentificator(std::map<std::string, FileID> files)
  : m_files(std::move(files))
{

}

FileID ThreadSafeFileIdentificator::getIdentification(const std::string& file)
{
  std::lock_guard lock{ m_mutex };
  return m_files.getIdentification(file);
}

std::vector<std::string> ThreadSafeFileIdentificator::getFiles() const
{
  std::lock_guard lock{ m_mutex };
  return m_files.getFiles();
}

} // namespace cppscanner

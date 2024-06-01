// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#include "basicfileidentificator.h"

namespace cppscanner
{

BasicFileIdentificator::BasicFileIdentificator(std::map<std::string, FileID> files)
  : m_files(std::move(files))
{
  m_files[""] = 0;
}

FileID BasicFileIdentificator::getIdentification(const std::string& file)
{
  auto it = m_files.find(file);

  if (it != m_files.end()) {
    return it->second;
  }

  auto result = FileID(m_files.size());
  m_files[file] = result;
  return result;
}

std::vector<std::string> BasicFileIdentificator::getFiles() const
{
  std::vector<std::string> result;
  result.resize(m_files.size());

  for (const auto& p : m_files) {
    result[p.second] = p.first;
  }

  return result;
}

} // namespace cppscanner

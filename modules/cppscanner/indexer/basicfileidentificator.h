// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_BASICFILEIDENTIFICATOR_H
#define CPPSCANNER_BASICFILEIDENTIFICATOR_H

#include "fileidentificator.h"

#include <map>

namespace cppscanner
{

class BasicFileIdentificator : public FileIdentificator
{
private:
  std::map<std::string, FileID> m_files;

public:
  explicit BasicFileIdentificator(std::map<std::string, FileID> files = {});

  FileID getIdentification(const std::string& file) final;
  std::vector<std::string> getFiles() const final;
};

} // namespace cppscanner

#endif // CPPSCANNER_BASICFILEIDENTIFICATOR_H

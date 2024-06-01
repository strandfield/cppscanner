// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_FILEIDENTIFICATOR_H
#define CPPSCANNER_FILEIDENTIFICATOR_H

#include "cppscanner/index/fileid.h"

#include <string>
#include <vector>

namespace cppscanner
{

class FileIdentificator
{
public:
  virtual ~FileIdentificator();

  virtual FileID getIdentification(const std::string& file) = 0;
  virtual std::vector<std::string> getFiles() const = 0;
  virtual std::string getFile(FileID fid) const;
};

} // namespace cppscanner

#endif // CPPSCANNER_FILEIDENTIFICATOR_H

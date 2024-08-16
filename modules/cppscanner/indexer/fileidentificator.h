// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_FILEIDENTIFICATOR_H
#define CPPSCANNER_FILEIDENTIFICATOR_H

#include "cppscanner/index/fileid.h"

#include <memory>
#include <string>
#include <vector>

namespace cppscanner
{

/**
 * \brief provides an integer-based identifier for files
 */
class FileIdentificator
{
public:
  virtual ~FileIdentificator();

  virtual FileID getIdentification(const std::string& file) = 0;
  virtual std::vector<std::string> getFiles() const = 0;
  virtual std::string getFile(FileID fid) const;

  static std::unique_ptr<FileIdentificator> createFileIdentificator();
  static std::unique_ptr<FileIdentificator> createThreadSafeFileIdentificator();
};

} // namespace cppscanner

#endif // CPPSCANNER_FILEIDENTIFICATOR_H

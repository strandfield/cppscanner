// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_FILE_H
#define CPPSCANNER_FILE_H

#include "fileid.h"

#include <string>

namespace cppscanner
{

/**
 * \brief raw struct for storing information about a file
 */
struct File
{
  FileID id;
  std::string path;
  std::string sha1;
  std::string content;
};

} // namespace cppscanner

#endif // CPPSCANNER_FILE_H

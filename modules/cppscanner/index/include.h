// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_INCLUDE_H
#define CPPSCANNER_INCLUDE_H

#include "fileid.h"

namespace cppscanner
{

/**
 * \brief stores information about a file #include
 */
struct Include
{
  /**
   * \brief the id of the file in which the include directive appears
   */
  FileID fileID;

  /**
   * \brief the id of the file that is being included
   */
  FileID includedFileID;

  /**
   * \brief the line number of the include directive
   */
  int line = -1;
};

} // namespace cppscanner

#endif // CPPSCANNER_INCLUDE_H

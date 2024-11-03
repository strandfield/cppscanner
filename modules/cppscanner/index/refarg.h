// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#pragma once

#include "fileid.h"
#include "fileposition.h"

namespace cppscanner
{

/**
 * \brief indicates a place where a function argument is passed by non-const reference
 */
struct ArgumentPassedByReference
{
  FileID fileID; //< id of the file in which the function is called
  FilePosition position; //< position of the argument passed by (non-const) reference
};

inline bool operator==(const ArgumentPassedByReference& lhs, const ArgumentPassedByReference& rhs)
{
  return lhs.fileID == rhs.fileID && lhs.position == rhs.position;
}

inline bool operator<(const ArgumentPassedByReference& lhs, const ArgumentPassedByReference& rhs)
{
  return (lhs.fileID < rhs.fileID) || (lhs.fileID == rhs.fileID && lhs.position < rhs.position);
}

} // namespace cppscanner

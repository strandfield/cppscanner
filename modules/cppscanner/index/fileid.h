// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_FILEID_H
#define CPPSCANNER_FILEID_H

#include <cstdint>

namespace cppscanner
{

using FileID = uint32_t;

inline FileID invalidFileID()
{
  return 0;
}

} // namespace cppscanner

#endif // CPPSCANNER_FILEID_H

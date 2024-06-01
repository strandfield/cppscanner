// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#include "fileidentificator.h"

namespace cppscanner
{

FileIdentificator::~FileIdentificator()
{

}

std::string FileIdentificator::getFile(FileID fid) const
{
  return getFiles().at(fid);
}

} // namespace cppscanner

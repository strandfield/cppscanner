// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_DECLARATION_H
#define CPPSCANNER_DECLARATION_H

#include "fileid.h"
#include "symbolid.h"
#include "fileposition.h"

namespace cppscanner
{

/**
 * \brief stores information about a symbol declaration
 */
struct SymbolDeclaration
{
  SymbolID symbolID; ///<  the id of the symbol that is declared
  FileID fileID; ///< the file in which the declaration occurs
  FilePosition startPosition; ///< the file in which the declaration occurs
  FilePosition endPosition; ///< the file in which the declaration occurs
  bool isDefinition; ///< whether the declaration is a definition
};

inline bool operator==(const SymbolDeclaration& lhs, const SymbolDeclaration& rhs)
{
  return lhs.symbolID == rhs.symbolID
    && lhs.fileID == rhs.fileID
    && lhs.startPosition == rhs.startPosition
    && lhs.endPosition == rhs.endPosition
    && lhs.isDefinition == rhs.isDefinition;
}

inline bool operator!=(const SymbolDeclaration& lhs, const SymbolDeclaration& rhs)
{
  return !(lhs == rhs);
}

} // namespace cppscanner

#endif // CPPSCANNER_DECLARATION_H

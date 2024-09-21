// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_REFERENCE_H
#define CPPSCANNER_REFERENCE_H

#include "fileid.h"
#include "symbolid.h"
#include "fileposition.h"

#include <string_view>

namespace cppscanner
{

/**
 * \brief stores information about a reference to a symbol
 */
struct SymbolReference
{
  /**
   * \brief the id of the symbol that is referenced
   */
  SymbolID symbolID;
  FileID fileID;
  FilePosition position;

  /**
   * \brief the id of the symbol that is referencing the referenced symbol
   */
  SymbolID referencedBySymbolID;

  /**
   * \brief a combination of flags as described by the Flag enum
   */
  int flags = 0;

  /**
   * \brief flag values for a symbol reference
   * 
   * This enum mirrors libclang's CXSymbolRole.
   */
  enum Flag
  {
    Declaration = 1 << 0,
    Definition = 1 << 1,
    Read = 1 << 2,
    Write = 1 << 3,
    Call = 1 << 4,
    Dynamic = 1 << 5, // declaration or call of virtual function
    AddressOf = 1 << 6,
    Implicit = 1 << 7
  };
};

inline bool operator==(const SymbolReference& lhs, const SymbolReference& rhs)
{
  return lhs.symbolID == rhs.symbolID
    && lhs.fileID == rhs.fileID
    && lhs.position == rhs.position
    && lhs.referencedBySymbolID == rhs.referencedBySymbolID
    && lhs.flags == rhs.flags;
}

inline bool operator!=(const SymbolReference& lhs, const SymbolReference& rhs)
{
  return !(lhs == rhs);
}

inline std::string_view getSymbolReferenceFlagString(SymbolReference::Flag flag)
{
  switch (flag) {
  case SymbolReference::Declaration: return "declaration";
  case SymbolReference::Definition: return "definition";
  case SymbolReference::Read: return "read";
  case SymbolReference::Write: return "write";
  case SymbolReference::Call: return "call";
  case SymbolReference::Dynamic: return "dynamic";
  case SymbolReference::AddressOf: return "addressof";
  case SymbolReference::Implicit: return "implicit";
  default: return "<invalid>";
  }
}

template<typename F>
void enumerateSymbolReferenceFlag(F&& fn)
{
  fn(SymbolReference::Declaration);
  fn(SymbolReference::Definition);
  fn(SymbolReference::Read);
  fn(SymbolReference::Write);
  fn(SymbolReference::Call);
  fn(SymbolReference::Dynamic);
  fn(SymbolReference::AddressOf);
  fn(SymbolReference::Implicit);
}

} // namespace cppscanner

#endif // CPPSCANNER_REFERENCE_H

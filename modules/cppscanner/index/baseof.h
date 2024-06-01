// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_BASEOF_H
#define CPPSCANNER_BASEOF_H

#include "access.h"
#include "relation.h"
#include "symbolid.h"

namespace cppscanner
{

class BaseOf
{
public:
  SymbolID baseClassID;
  SymbolID derivedClassID;
  AccessSpecifier accessSpecifier;

public:
  Relation toRelation() const;
};

inline Relation BaseOf::toRelation() const
{
  return Relation(baseClassID, RelationKind::BaseOf, derivedClassID);
}

} // namespace cppscanner

#endif // CPPSCANNER_BASEOF_H

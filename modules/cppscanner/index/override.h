// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_OVERRIDE_H
#define CPPSCANNER_OVERRIDE_H

#include "relation.h"
#include "symbolid.h"

namespace cppscanner
{

class Override
{
public:
  SymbolID baseMethodID;
  SymbolID overrideMethodID;

public:
  Relation toRelation() const;
};

inline Relation Override::toRelation() const
{
  return Relation(baseMethodID, RelationKind::OverriddenBy, overrideMethodID);
}

} // namespace cppscanner

#endif // CPPSCANNER_OVERRIDE_H

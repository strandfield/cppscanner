// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_OVERRIDE_H
#define CPPSCANNER_OVERRIDE_H

#include "relation.h"
#include "symbolid.h"

namespace cppscanner
{

/**
 * \brief represents an "overrides" relation between two member functions
 */
class Override
{
public:
  SymbolID baseMethodID; //< the id of the virtual method in the base class
  SymbolID overrideMethodID; //< the id of the method in the derived class

public:
  Relation toRelation() const;
};

inline Relation Override::toRelation() const
{
  return Relation(baseMethodID, RelationKind::OverriddenBy, overrideMethodID);
}

} // namespace cppscanner

#endif // CPPSCANNER_OVERRIDE_H

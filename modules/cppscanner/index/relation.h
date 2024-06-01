// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_RELATION_H
#define CPPSCANNER_RELATION_H

#include "symbolid.h"

#include <string>
#include <string_view>
#include <tuple>

namespace cppscanner
{

enum class RelationKind : uint8_t {
  BaseOf = 1,
  OverriddenBy = 2,
};

std::string_view getRelationKindString(RelationKind rel);

template<typename F>
void enumerateRelationKind(F&& fn)
{
  fn(RelationKind::BaseOf);
  fn(RelationKind::OverriddenBy);
}

class Relation 
{
public:
  SymbolID subject;
  RelationKind predicate;
  SymbolID object;

public:
  Relation() = default;

  Relation(SymbolID theSubject, RelationKind thePredicate, SymbolID theObject);
};

inline Relation::Relation(SymbolID theSubject, RelationKind thePredicate, SymbolID theObject) :
  subject(theSubject),
  predicate(thePredicate),
  object(theObject)
{

}

inline bool operator==(const Relation& lhs, const Relation& rhs)  
{
  return std::forward_as_tuple(lhs.subject, lhs.predicate, lhs.object) ==
    std::forward_as_tuple(rhs.subject, rhs.predicate, rhs.object);
}

inline bool operator<(const Relation& lhs, const Relation& rhs)
{
  return std::forward_as_tuple(lhs.subject, lhs.predicate, lhs.object) <
    std::forward_as_tuple(rhs.subject, rhs.predicate, rhs.object);
}

} // namespace cppscanner

#endif // CPPSCANNER_RELATION_H

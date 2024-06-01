// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#include "relation.h"


namespace cppscanner
{

std::string_view getRelationKindString(RelationKind rel)
{
  switch (rel)
  {
  case RelationKind::BaseOf: return "base-of";
  case RelationKind::OverriddenBy: return "overridden-by";
  default: return "<invalid>";
  }
}

} // namespace cppscanner

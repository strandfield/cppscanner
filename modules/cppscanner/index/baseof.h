// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_BASEOF_H
#define CPPSCANNER_BASEOF_H

#include "access.h"
#include "symbolid.h"

namespace cppscanner
{

/**
 * \brief represents a "base of" relation between two classes
 */
struct BaseOf
{
  SymbolID baseClassID; //< the id of the base class
  SymbolID derivedClassID; //< the id of the derived class
  AccessSpecifier accessSpecifier; //< whether public, protected or private inheritance is used
};

} // namespace cppscanner

#endif // CPPSCANNER_BASEOF_H

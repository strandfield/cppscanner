// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#include "symbol.h"

#include <stdexcept>
#include <vector>

namespace cppscanner
{

std::string_view getSymbolKindString(SymbolKind w)
{
  static const std::vector<std::string> strs = {
    "<unknown>",
    "module",
    "namespace",
    "namespace-alias",
    "macro",
    "enum",
    "struct",
    "class",
    "union",
    "lambda",
    "type-alias",
    "function",
    "variable",
    "field",
    "enum-constant",
    "instance-method",
    "class-method",
    "static-method",
    "static-property",
    "constructor",
    "destructor",
    "conversion-function",
    "parameter",
    "using",
    "template-type-parameter",
    "template-template-parameter",
    "non-type-template-parameter",
    "concept",
  };

  auto i = static_cast<size_t>(w);

  if (i >= strs.size())
    return "";
  else
    return strs.at(i);
}

Symbol::Symbol(SymbolKind w, std::string n, Symbol* parent) :
  kind(w),
  name(std::move(n)),
  parent_id(parent ? parent->id : SymbolID())
{

}

} // namespace cppscanner

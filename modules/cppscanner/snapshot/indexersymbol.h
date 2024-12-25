// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_INDEXERSYMBOL_H
#define CPPSCANNER_INDEXERSYMBOL_H

#include "cppscanner/index/symbol.h"

#include <variant>

namespace cppscanner
{

class IndexerSymbol : public SymbolRecord
{
public:
  std::variant<std::monostate, MacroInfo, FunctionInfo, ParameterInfo, EnumInfo, EnumConstantInfo, VariableInfo, NamespaceAliasInfo> extraInfo;

  template<typename T>
  T& getExtraInfo()
  {
    if (std::holds_alternative<std::monostate>(extraInfo))
    {
      extraInfo = T();
    }

    return std::get<T>(extraInfo);
  }

  bool testFlag(int f) const
  {
    return flags & f;
  }

  void setFlag(int f, bool on = true)
  {
    if (on)
      flags |= f;
    else
      flags &= ~((int)f);
  }

  void setLocal(bool on = true)
  {
    setFlag(SymbolFlag::Local, on);
  }

  enum Update
  {
    FlagUpdate
  };
};

inline int update(IndexerSymbol& symbol, const SymbolRecord& other)
{
  int what = 0;

  if (symbol.flags != other.flags) {
    // TODO: this may not be the right way to update the flags
    symbol.flags |= other.flags;
    what |= IndexerSymbol::FlagUpdate;
  }

  // TODO: there may be other things to update...

  return  what;
}

} // namespace cppscanner

#endif // CPPSCANNER_INDEXERSYMBOL_H

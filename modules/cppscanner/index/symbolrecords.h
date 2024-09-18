// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_SYMBOLRECORDS_H
#define CPPSCANNER_SYMBOLRECORDS_H

#include "access.h"
#include "symbolid.h"
#include "symbolkind.h"

#include <string>

namespace cppscanner
{

struct SymbolFlag
{
  enum Value
  {
    Local         = 0x0001,
    FromProject   = 0x0002, // TODO: change name ?
    Protected     = 0x0004,
    Private       = 0x0008,
    Reserved1     = 0x0010,
    MinCustomFlag = 0x0020
  };
};

/**
 * \brief stores basic information about a symbol
 */
struct SymbolRecord
{
  SymbolID id; //< the id of the symbol
  SymbolKind kind; //< what kind of symbol is this
  std::string name; //< the name of the symbol
  SymbolID parentId; //< the id of the symbol's parent
  int flags = 0; //< OR-combination of flags
};

inline bool testFlag(const SymbolRecord& record, int f)
{
  return record.flags & f;
}

/**
 * \brief stores extra information about a macro
 */
struct MacroInfo
{
  enum Flag
  {
    MacroUsedAsHeaderGuard = 0x0020,
    FunctionLike           = 0x0040
  };

  static_assert(MacroUsedAsHeaderGuard == SymbolFlag::MinCustomFlag);

  std::string definition;
};

struct MacroRecord : SymbolRecord, MacroInfo { };

struct NamespaceAliasInfo
{
  std::string value;
};

struct NamespaceAliasRecord : SymbolRecord, NamespaceAliasInfo { };

struct VariableInfo
{
  enum Flag
  {
    Const       = 0x0020,
    Constexpr   = 0x0040,
    Static      = 0x0080,
    Mutable     = 0x0100,
    ThreadLocal = 0x0200,
    Inline      = 0x0400,
  };

  static_assert(Const == SymbolFlag::MinCustomFlag);

  std::string type;
  std::string init;
};

struct VariableRecord : SymbolRecord, VariableInfo { };

struct ParameterInfo
{
  int parameterIndex;
  std::string type;
  std::string defaultValue;
};

struct ParameterRecord : SymbolRecord, ParameterInfo { };

struct FunctionInfo
{
  enum Flag
  {
    Inline    = 0x00020,
    Static    = 0x00040,
    Constexpr = 0x00080,
    Consteval = 0x00100,
    Noexcept  = 0x00200,   
    Default   = 0x00400,
    Delete    = 0x00800,
    Const     = 0x01000,
    Virtual   = 0x02000,
    Pure      = 0x04000,
    Override  = 0x08000,
    Final     = 0x10000,
    Explicit  = 0x20000,
  };

  static_assert(Inline == SymbolFlag::MinCustomFlag);

  std::string returnType;
  std::string declaration;
};

struct FunctionRecord : SymbolRecord, FunctionInfo { };

struct EnumInfo
{
  std::string underlyingType;
};

struct EnumRecord : SymbolRecord, EnumInfo { };

struct EnumConstantInfo
{
  int64_t value;
  std::string expression;
};

struct EnumConstantRecord : SymbolRecord, EnumConstantInfo { };

struct ClassInfo
{
  enum Flag
  {
    Final = 0x00020
  };

  static_assert(Final == SymbolFlag::MinCustomFlag);
};

} // namespace cppscanner

#endif // CPPSCANNER_SYMBOLRECORDS_H

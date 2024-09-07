// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_SYMBOL_H
#define CPPSCANNER_SYMBOL_H

#include "access.h"
#include "symbolid.h"

#include <string>
#include <string_view>

namespace cppscanner
{

// TODO: reorder, add inline-namespace, scoped-enum, operator
/**
 * \brief enum describing the kind of a symbol
 */
enum class SymbolKind
{
  Unknown = 0,

  Module,
  Namespace,
  NamespaceAlias,

  Macro,

  Enum,
  Struct,
  Class,
  Union,
  /**
   * clang reports lambda functions as a class.
   * indeed, a lambda is just syntactic sugar for a class.
   * but at the user level, it seems convenient to distinguish
   * a "true" class from a lambda.
   */
  Lambda,

  TypeAlias,

  Function,
  Variable,
  Field,
  EnumConstant,

  InstanceMethod,
  ClassMethod, // TODO: remove me, afaik this is for Objective-C
  StaticMethod,
  StaticProperty,

  Constructor,
  Destructor,
  ConversionFunction,
  Parameter,
  /**
   * represents a symbol introduced/created by a using-declaration 
   * 
   * 'using' may be used to produce a class constructor as in:
   * class Derived : public Base {
   * public: using Base::Base;
   * };
   * 
   * TODO: should this case be further processed and be referenced as a constructor ?
   * TODO: test what happens when using refers to a method and not the constructor
   */
  Using,
  TemplateTypeParameter,
  TemplateTemplateParameter,
  NonTypeTemplateParameter,

  Concept,
};

std::string_view getSymbolKindString(SymbolKind w);

template<typename F>
void enumerateSymbolKind(F&& fn)
{
  fn(SymbolKind::Unknown);

  fn(SymbolKind::Module);
  fn(SymbolKind::Namespace);
  fn(SymbolKind::NamespaceAlias);

  fn(SymbolKind::Macro);

  fn(SymbolKind::Enum);
  fn(SymbolKind::Struct);
  fn(SymbolKind::Class);
  fn(SymbolKind::Union);
  fn(SymbolKind::Lambda);

  fn(SymbolKind::TypeAlias);

  fn(SymbolKind::Function);
  fn(SymbolKind::Variable);
  fn(SymbolKind::Field);
  fn(SymbolKind::EnumConstant);

  fn(SymbolKind::InstanceMethod);
  fn(SymbolKind::ClassMethod);
  fn(SymbolKind::StaticMethod);
  fn(SymbolKind::StaticProperty);

  fn(SymbolKind::Constructor);
  fn(SymbolKind::Destructor);
  fn(SymbolKind::ConversionFunction);
  fn(SymbolKind::Parameter);
  fn(SymbolKind::Using);
  fn(SymbolKind::TemplateTypeParameter);
  fn(SymbolKind::TemplateTemplateParameter);
  fn(SymbolKind::NonTypeTemplateParameter);

  fn(SymbolKind::Concept);
}

struct SymbolFlag
{
  enum Value
  {
    Local         = 0x0001,
    Protected     = 0x0002,
    Private       = 0x0004,
    Reserved1     = 0x0008,
    Reserved2     = 0x0010,
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

struct NamespaceInfo
{
  enum Flag
  {
    Inline = 0x0020,
  };

  static_assert(Inline == SymbolFlag::MinCustomFlag);
};

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
  enum Flag
  {
    IsScoped    = 0x00020,
  };

  static_assert(IsScoped == SymbolFlag::MinCustomFlag);


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
    Final = SymbolFlag::MinCustomFlag
  };

  static_assert(Final == SymbolFlag::MinCustomFlag);
};

} // namespace cppscanner

#endif // CPPSCANNER_SYMBOL_H

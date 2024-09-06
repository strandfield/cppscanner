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

using MacroRecord = struct : SymbolRecord, MacroInfo { };

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

/**
 * \brief represents a symbol in a C++ program
 */
 class [[deprecated]] Symbol
{
public:
  SymbolID id;
  SymbolKind kind = SymbolKind::Unknown;
  std::string name;
  SymbolID parent_id;
  int flags = 0;
  // TODO: should the following fields be moved to another "extra info" class ?
  int parameterIndex = -1;
  std::string type;
  std::string value;

  // TODO: merge some flags that are mutually exclusive
  // e.g. IsScoped only applies to enum and therefore cannot appear at the same time as many other flags
  // maybe we could distinguish the flags by the kind of symbol for which they apply:
  // there would be flags for functions, classes, enums, etc...
  // 
  enum Flag
  {
    Local     = 0x0001,
    Public    = 0x0002, // TODO: remove me? just have a flag for protected and private
    Protected = 0x0004,
    Private   = 0x0006,
    Inline    = 0x0008,
    Static    = 0x0010,
    Constexpr = 0x0020,
    IsScoped  = 0x0040,
    // TODO: we could use fewer bits for virtual, override, final and pure.
    // unless we want to distinguish between what is written and what is
    Virtual   = 0x0080,
    Override  = 0x0100,
    Final     = 0x0200,
    Const     = 0x0400,
    Pure      = 0x0800,
    Noexcept  = 0x1000,
    Explicit  = 0x2000,
    Default   = 0x4000,
    Delete    = 0x8000,
    // TODO: we need to redesign flags...
    MacroUsedAsHeaderGuard = 0x10000,
    // TODO:
    Mutable = 0,
    ThreadLocal = 0,
    Consteval = 0,
    Constinit = 0,
    Exported = 0, // ? export of a C++ module
  };

public:
  Symbol() = default;
  explicit Symbol(SymbolKind w, std::string name, Symbol* parent = nullptr);

  bool testFlag(Symbol::Flag f) const;
  bool testFlag(int f) const;
  void setFlag(Symbol::Flag f, bool on = true);

  void setLocal(bool on = true);
};

inline bool Symbol::testFlag(Symbol::Flag f) const
{
  return flags & f;
}

inline bool Symbol::testFlag(int f) const
{
  return flags & f;
}

inline void Symbol::setFlag(Symbol::Flag f, bool on)
{
  if (on)
    flags |= f;
  else
    flags &= ~((int)f);
}

inline void Symbol::setLocal(bool on)
{
  setFlag(Flag::Local, on);
}

inline bool is_local(const Symbol& s)
{
  return s.flags & Symbol::Local;
}

[[deprecated]] inline bool test_flag(const Symbol& s, Symbol::Flag f)
{
  return s.testFlag(f);
}

[[deprecated]] inline void set_flag(Symbol& s, Symbol::Flag f, bool on = true)
{
  s.setFlag(f, on);
}

inline AccessSpecifier get_access_specifier(const Symbol& s)
{
  switch (s.flags & Symbol::Private)
  {
  case Symbol::Public:
    return AccessSpecifier::Public;
  case Symbol::Protected:
    return AccessSpecifier::Protected;
  case Symbol::Private:
    return AccessSpecifier::Private;
  default:
    return AccessSpecifier::Invalid;
  }
}

inline void set_access_specifier(Symbol& s, AccessSpecifier a)
{
  s.flags = s.flags & ~Symbol::Private;

  switch (a)
  {
  case AccessSpecifier::Public:
    s.flags |= Symbol::Public;
    break;
  case AccessSpecifier::Protected:
    s.flags |= Symbol::Protected;
    break;
  case AccessSpecifier::Private:
    s.flags |= Symbol::Private;
    break;
  }
}

inline std::string_view getSymbolFlagString(Symbol::Flag flag)
{
  switch (flag) {
  case Symbol::Local:                  return "local";
  case Symbol::Public:                 return "public";
  case Symbol::Protected:              return "protected";
  case Symbol::Private:                return "private";
  case Symbol::Inline:                 return "inline";
  case Symbol::Static:                 return "static";
  case Symbol::Constexpr:              return "constexpr";
  case Symbol::IsScoped:               return "scoped";
  case Symbol::Virtual:                return "virtual";
  case Symbol::Override:               return "override";
  case Symbol::Final:                  return "final";
  case Symbol::Const:                  return "const";
  case Symbol::Pure:                   return "pure";
  case Symbol::Noexcept:               return "noexcept";
  case Symbol::Explicit:               return "explicit";
  case Symbol::Default:                return "default";
  case Symbol::Delete:                 return "delete";
  case Symbol::MacroUsedAsHeaderGuard: return "macro-used-as-header-guard";
  default: return "<invalid>";
  }
}

template<typename F>
void enumerateSymbolFlag(F&& fn)
{
  fn(Symbol::Local);
  fn(Symbol::Public);
  fn(Symbol::Protected);
  fn(Symbol::Private);
  fn(Symbol::Inline);
  fn(Symbol::Static);
  fn(Symbol::Constexpr);
  fn(Symbol::IsScoped);
  fn(Symbol::Virtual);
  fn(Symbol::Override);
  fn(Symbol::Final);
  fn(Symbol::Const);
  fn(Symbol::Pure);
  fn(Symbol::Noexcept);
  fn(Symbol::Explicit);
  fn(Symbol::Default);
  fn(Symbol::Delete);
  fn(Symbol::MacroUsedAsHeaderGuard);
}

} // namespace cppscanner

#endif // CPPSCANNER_SYMBOL_H

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
  ClassMethod,
  StaticMethod,
  StaticProperty,

  Constructor,
  Destructor,
  ConversionFunction,
  Parameter,
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

class Symbol
{
public:
  SymbolID id;
  SymbolKind kind = SymbolKind::Unknown;
  std::string name;
  std::string display_name; // could be left empty to save space ?
  SymbolID parent_id;
  int flags = 0;
  int parameterIndex = -1;
  std::string type;
  std::string value;

  enum Flag
  {
    Local     = 0x0001,
    Public    = 0x0002,
    Protected = 0x0004,
    Private   = 0x0006,
    Inline    = 0x0008,
    Static    = 0x0010,
    Constexpr = 0x0020,
    IsScoped  = 0x0040,
    Virtual   = 0x0080,
    Override  = 0x0100,
    Final     = 0x0200,
    Const     = 0x0400,
    Pure      = 0x0800,
    Noexcept  = 0x1000,
    Explicit  = 0x2000,
    Default   = 0x4000,
    Delete    = 0x8000,
  };

public:
  Symbol() = default;
  explicit Symbol(SymbolKind w, std::string name, Symbol* parent = nullptr);

  bool testFlag(Symbol::Flag f) const;
  void setFlag(Symbol::Flag f, bool on = true);

  void setLocal(bool on = true);
};

inline bool Symbol::testFlag(Symbol::Flag f) const
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
  case Symbol::Local:     return "local";
  case Symbol::Public:    return "public";
  case Symbol::Protected: return "protected";
  case Symbol::Private:   return "private";
  case Symbol::Inline:    return "inline";
  case Symbol::Static:    return "static";
  case Symbol::Constexpr: return "constexpr";
  case Symbol::IsScoped:  return "scoped";
  case Symbol::Virtual:   return "virtual";
  case Symbol::Override:  return "override";
  case Symbol::Final:     return "final";
  case Symbol::Const:     return "const";
  case Symbol::Pure:      return "pure";
  case Symbol::Noexcept:  return "noexcept";
  case Symbol::Explicit:  return "explicit";
  case Symbol::Default:   return "default";
  case Symbol::Delete:    return "delete";
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
}

} // namespace cppscanner

#endif // CPPSCANNER_SYMBOL_H

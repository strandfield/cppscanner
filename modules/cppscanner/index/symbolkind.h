// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_SYMBOLKIND_H
#define CPPSCANNER_SYMBOLKIND_H

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

} // namespace cppscanner

#endif // CPPSCANNER_SYMBOLKIND_H

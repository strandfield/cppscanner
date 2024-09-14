// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_SYMBOLKIND_H
#define CPPSCANNER_SYMBOLKIND_H

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
  InlineNamespace,
  NamespaceAlias,

  Macro,

  Enum,
  EnumClass,
  Struct,
  Class,
  Union,
  Lambda,

  Typedef,
  TypeAlias,

  EnumConstant,

  Variable,
  Field,
  StaticProperty,

  Function,
  Method,
  StaticMethod,
  Constructor,
  Destructor,
  Operator,
  ConversionFunction,

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

  Parameter,

  TemplateTypeParameter,
  TemplateTemplateParameter,
  NonTypeTemplateParameter,

  Concept,
};

inline std::string_view getSymbolKindString(SymbolKind w)
{
  switch (w)
  {
  case SymbolKind::Unknown:                   return "<unknown>";
  case SymbolKind::Module:                    return "module";
  case SymbolKind::Namespace:                 return "namespace";
  case SymbolKind::InlineNamespace:           return "inline-namespace";
  case SymbolKind::NamespaceAlias:            return "namespace-alias";
  case SymbolKind::Macro:                     return "macro";
  case SymbolKind::Enum:                      return "enum";
  case SymbolKind::EnumClass:                 return "enum-class";
  case SymbolKind::Struct:                    return "struct";
  case SymbolKind::Class:                     return "class";
  case SymbolKind::Union:                     return "union";
  case SymbolKind::Lambda:                    return "lambda";
  case SymbolKind::Typedef:                   return "typedef";
  case SymbolKind::TypeAlias:                 return "type-alias";
  case SymbolKind::EnumConstant:              return "enum-constant";
  case SymbolKind::Variable:                  return "variable";
  case SymbolKind::Field:                     return "field";
  case SymbolKind::StaticProperty:            return "static-property";
  case SymbolKind::Function:                  return "function";
  case SymbolKind::Method:                    return "method";
  case SymbolKind::StaticMethod:              return "static-method";
  case SymbolKind::Constructor:               return "constructor";
  case SymbolKind::Destructor:                return "destructor";
  case SymbolKind::Operator:                  return "operator";
  case SymbolKind::ConversionFunction:        return "conversion-function";
  case SymbolKind::Using:                     return "using";
  case SymbolKind::Parameter:                 return "parameter";
  case SymbolKind::TemplateTypeParameter:     return "template-type-parameter";
  case SymbolKind::TemplateTemplateParameter: return "template-template-parameter";
  case SymbolKind::NonTypeTemplateParameter:  return "non-type-template-parameter";
  case SymbolKind::Concept:                   return "concept";
  default:                                    return "<unknown>";
  }
}

template<typename F>
void enumerateSymbolKind(F&& fn)
{
  fn(SymbolKind::Unknown);

  fn(SymbolKind::Module);

  fn(SymbolKind::Namespace);
  fn(SymbolKind::InlineNamespace);
  fn(SymbolKind::NamespaceAlias);

  fn(SymbolKind::Macro);

  fn(SymbolKind::Enum);
  fn(SymbolKind::EnumClass);
  fn(SymbolKind::Struct);
  fn(SymbolKind::Class);
  fn(SymbolKind::Union);
  fn(SymbolKind::Lambda);

  fn(SymbolKind::Typedef);
  fn(SymbolKind::TypeAlias);

  fn(SymbolKind::EnumConstant);

  fn(SymbolKind::Variable);
  fn(SymbolKind::Field);
  fn(SymbolKind::StaticProperty);

  fn(SymbolKind::Function);
  fn(SymbolKind::Method);
  fn(SymbolKind::StaticMethod);
  fn(SymbolKind::Constructor);
  fn(SymbolKind::Destructor);
  fn(SymbolKind::Operator);
  fn(SymbolKind::ConversionFunction);

  fn(SymbolKind::Using);

  fn(SymbolKind::Parameter);

  fn(SymbolKind::TemplateTypeParameter);
  fn(SymbolKind::TemplateTemplateParameter);
  fn(SymbolKind::NonTypeTemplateParameter);

  fn(SymbolKind::Concept);
}

} // namespace cppscanner

#endif // CPPSCANNER_SYMBOLKIND_H

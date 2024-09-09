// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#include "indexer.h"

#include "fileidentificator.h"

#include "cppscanner/indexer/fileindexingarbiter.h"
#include "cppscanner/indexer/indexingresultqueue.h"
#include "cppscanner/index/symbolid.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Type.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Index/USRGeneration.h>
#include <clang/Lex/PreprocessingRecord.h>

#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/SHA1.h>

#include <cassert>
#include <iostream>

namespace cppscanner
{

SymbolID usr2symid(const std::string& usr)
{
  std::array<uint8_t, 20> sha1 = llvm::SHA1::hash(llvm::arrayRefFromStringRef(usr));
  static_assert(sizeof(sha1) >= sizeof(SymbolID::value_type));
  SymbolID::value_type rawid;
  memcpy(&rawid, sha1.data(), sizeof(SymbolID::value_type));
  return SymbolID::fromRawID(rawid);
}

std::string getUSR(const clang::Decl* decl)
{
  llvm::SmallVector<char> chars;
  
  // Weird api... I got it wrong the first time.
  // Returns true on failure :O
  bool ignore = clang::index::generateUSRForDecl(decl, chars);

  if (ignore) {
    throw std::runtime_error("could not generate usr");
  }

  return std::string(chars.data(), chars.size());
}

std::string getUSR(const clang::IdentifierInfo* name, const clang::MacroInfo* macroInfo, const clang::SourceLocation& loc, const clang::SourceManager& sourceManager)
{
  llvm::SmallVector<char> chars;

  bool ignore = clang::index::generateUSRForMacro(name->getName(), loc, sourceManager, chars);

  if (ignore) {
    throw std::runtime_error("could not generate usr");
  }

  return std::string(chars.data(), chars.size());
}

SymbolKind tr(const clang::index::SymbolKind k)
{
  switch (k)
  {
  case clang::index::SymbolKind::Unknown: return SymbolKind::Unknown;
  case clang::index::SymbolKind::Module: return SymbolKind::Module;
  case clang::index::SymbolKind::Namespace: return SymbolKind::Namespace;
  case clang::index::SymbolKind::NamespaceAlias: return SymbolKind::NamespaceAlias;
  case clang::index::SymbolKind::Macro: return SymbolKind::Macro;
  case clang::index::SymbolKind::Enum: return SymbolKind::Enum;
  case clang::index::SymbolKind::Struct: return SymbolKind::Struct;
  case clang::index::SymbolKind::Class: return SymbolKind::Class;
  case clang::index::SymbolKind::Protocol: return SymbolKind::Class; // MS __interface
  case clang::index::SymbolKind::Union: return SymbolKind::Class;
  case clang::index::SymbolKind::TypeAlias: return SymbolKind::TypeAlias;
  case clang::index::SymbolKind::Function: return SymbolKind::Function;
  case clang::index::SymbolKind::Variable: return SymbolKind::Variable;
  case clang::index::SymbolKind::Field: return SymbolKind::Field;
  case clang::index::SymbolKind::EnumConstant: return SymbolKind::EnumConstant;
  case clang::index::SymbolKind::InstanceMethod: return SymbolKind::Method;
  case clang::index::SymbolKind::StaticMethod: return SymbolKind::StaticMethod;
  case clang::index::SymbolKind::StaticProperty: return SymbolKind::StaticProperty;
  case clang::index::SymbolKind::Constructor: return SymbolKind::Constructor;
  case clang::index::SymbolKind::Destructor: return SymbolKind::Destructor;
  case clang::index::SymbolKind::ConversionFunction: return SymbolKind::ConversionFunction;
  case clang::index::SymbolKind::Parameter: return SymbolKind::Parameter;
  case clang::index::SymbolKind::Using: return SymbolKind::Using;
  case clang::index::SymbolKind::TemplateTypeParm: return SymbolKind::TemplateTypeParameter;
  case clang::index::SymbolKind::TemplateTemplateParm: return SymbolKind::TemplateTemplateParameter;
  case clang::index::SymbolKind::NonTypeTemplateParm: return SymbolKind::NonTypeTemplateParameter;
  case clang::index::SymbolKind::Concept: return SymbolKind::Concept;
  case clang::index::SymbolKind::Extension: [[fallthrough]]; // Obj-C
  case clang::index::SymbolKind::InstanceProperty: [[fallthrough]]; // MS C++ property (https://learn.microsoft.com/en-us/cpp/cpp/property-cpp?view=msvc-170)
  case clang::index::SymbolKind::ClassMethod: [[fallthrough]]; // Objective-C class method
  case clang::index::SymbolKind::ClassProperty: [[fallthrough]]; // AFAIK, unused
  default: return SymbolKind::Unknown;
  }
}

SymbolID computeSymbolIdFromUsr(const std::string& usr)
{
  return usr2symid(usr);
}

SymbolID computeSymbolID(const clang::Decl* decl)
{
  std::string usr = getUSR(decl);
  return computeSymbolIdFromUsr(usr);
}

clang::PrintingPolicy getPrettyPrintPrintingPolicy(const Indexer& idxr)
{
  clang::PrintingPolicy printpol = idxr.getAstContext()->getPrintingPolicy();
  printpol.TerseOutput = true;
  printpol.PolishForDeclaration = true;
  printpol.IncludeNewlines = false;
  return printpol;
}

std::string prettyPrint(const clang::QualType& type, const Indexer& idxr)
{
  clang::PrintingPolicy pp = getPrettyPrintPrintingPolicy(idxr);
  return type.getAsString(pp);
}

std::string prettyPrint(const clang::Expr* expr, const Indexer& idxr)
{
  llvm::SmallString<64> str;
  llvm::raw_svector_ostream os{ str };

  expr->printPretty(os, nullptr, getPrettyPrintPrintingPolicy(idxr), 0, " ", idxr.getAstContext());

  return os.str().str();
}

// note: would also work with a clang::Decl
std::string prettyPrint(const clang::FunctionDecl& decl, const Indexer& idxr)
{
  llvm::SmallString<64> str;
  llvm::raw_svector_ostream os{ str };

  decl.print(os, getPrettyPrintPrintingPolicy(idxr));
  
  return os.str().str();
}

std::string prettyPrint(const clang::NestedNameSpecifier& nns, const Indexer& idxr)
{
  llvm::SmallString<64> str;
  llvm::raw_svector_ostream os{ str };

  nns.print(os, getPrettyPrintPrintingPolicy(idxr));

  return os.str().str();
}

inline std::string to_string(const llvm::APSInt& value)
{
  llvm::SmallString<64> str;
  value.toString(str);
  return str.str().str();
}

inline static void remove_space_before_ref(std::string& str)
{
  if (str.find(" &", str.size() - 2) != std::string::npos)
  {
    str.erase(str.size() - 2, 1);
  }
  else if (str.find(" &&", str.size() - 3) != std::string::npos)
  {
    str.erase(str.size() - 3, 1);
  }
}

std::string computeName(const clang::FunctionDecl& decl, const Indexer& idxr)
{
  std::string ret = decl.getNameInfo().getAsString();

  ret.push_back('(');

  bool first_param = true;

  for (const clang::ParmVarDecl* param : decl.parameters())
  {
    if (!first_param) {
      ret += ", ";
    } else {
      first_param = false;
    }

    ret += prettyPrint(param->getOriginalType(), idxr);

    remove_space_before_ref(ret);
  }

  ret.push_back(')');

  if (auto* mdecl = llvm::dyn_cast<clang::CXXMethodDecl>(&decl))
  {
    if (mdecl->isConst()) {
      ret += " const";
    }
  }

  if (clang::isNoexceptExceptionSpec(decl.getExceptionSpecType())) {
    ret += " noexcept";
  }

  return ret;
}

void fillEmptyRecordName(IndexerSymbol& symbol, const clang::RecordDecl& decl)
{
  (void)decl; // we are not going to use 'decl' for now

  switch (symbol.kind)
  {
  case SymbolKind::Lambda:
    symbol.name = "__lambda_" + symbol.id.toHex();
    break;
  case SymbolKind::Struct:
    symbol.name = "__anonymous_struct_" + symbol.id.toHex();
    break;
  case SymbolKind::Class:
    symbol.name = "__anonymous_class_" + symbol.id.toHex();
    break;
  case SymbolKind::Union:
    symbol.name = "__anonymous_union_" + symbol.id.toHex();
    break;
  default:
    break;
  }
}

SymbolCollector::SymbolCollector(Indexer& idxr) : 
  m_indexer(idxr)
{

}

void SymbolCollector::reset()
{
  m_symbolIdCache.clear();
}

IndexerSymbol* SymbolCollector::process(const clang::Decl* decl)
{
  auto [it, inserted] = m_symbolIdCache.try_emplace(decl, SymbolID());

  if (inserted) {
    try {
      it->second = computeSymbolID(decl);
    } catch (const std::exception&) {
      return nullptr;
    }
  };

  SymbolID symid = it->second;
  IndexerSymbol& symbol = m_indexer.getCurrentIndex()->symbols[symid];

  if (inserted) {
    symbol.id = symid;

    /*
    if (m_indexer.ShouldTraverseDecl(decl)) {
      fillSymbol(symbol, decl);
    } else {
      // do minimal stuff
      symbol.name = getDeclSpelling(decl);
    }
    */
    fillSymbol(symbol, decl);
  }

  return &symbol;
}

IndexerSymbol* SymbolCollector::process(const clang::IdentifierInfo* name, const clang::MacroInfo* macroInfo, clang::SourceLocation loc)
{
  auto [it, inserted] = m_symbolIdCache.try_emplace(macroInfo, SymbolID());

  if (inserted) {
    try {
      it->second = computeSymbolIdFromUsr(getUSR(name, macroInfo, loc, m_indexer.getAstContext()->getSourceManager()));
    } catch (const std::exception&) {
      return nullptr;
    }
  };

  SymbolID symid = it->second;
  IndexerSymbol& symbol = m_indexer.getCurrentIndex()->symbols[symid];

  if (inserted) {
    symbol.id = symid;
    fillSymbol(symbol, name, macroInfo);
  }

  // note: it seems we have to delay the call to isUsedForHeaderGuard()
  // as its value isn't set the first time handleMacroOccurrence() is called;
  // which is not completely illogical as there is no way to know yet if
  // the macro is really a header guard.
  // PreprocessingRecordIndexer takes care of reading the value after the 
  // translation unit has been processed.
  (void)macroInfo->isUsedForHeaderGuard();

  symbol.setFlag(MacroInfo::FunctionLike, macroInfo->isFunctionLike());

  return &symbol;
}

SymbolID SymbolCollector::getSymbolId(const clang::Decl* decl) const
{
  auto it = m_symbolIdCache.find(decl);
  return it != m_symbolIdCache.end() ? it->second : SymbolID();
}

SymbolID SymbolCollector::getMacroSymbolIdFromCache(const clang::MacroInfo* macroInfo) const
{
  auto it = m_symbolIdCache.find(macroInfo);
  return it != m_symbolIdCache.end() ? it->second : SymbolID();
}

std::string SymbolCollector::getDeclSpelling(const clang::Decl* decl)
{
  auto* nd = llvm::dyn_cast<clang::NamedDecl>(decl);

  if (!nd) {
    return {};
  }

  return nd->getNameAsString();
}

void SymbolCollector::fillSymbol(IndexerSymbol& symbol, const clang::Decl* decl)
{
  if (symbol.name.empty()) {
    symbol.name = getDeclSpelling(decl);
  }

  clang::index::SymbolInfo info = clang::index::getSymbolInfo(decl);
  
  symbol.kind = tr(info.Kind);

  if (const auto* fun = llvm::dyn_cast<clang::FunctionDecl>(decl)) 
  {
    symbol.name = computeName(*fun, m_indexer);
  }

  if (info.Properties & (clang::index::SymbolPropertySet)clang::index::SymbolProperty::Local) {
    symbol.setLocal();
  }

  if (symbol.kind == SymbolKind::TemplateTypeParameter || symbol.kind == SymbolKind::NonTypeTemplateParameter) {
    // TODO: clang does not seem to consider a class template parameter as being local.
    // is it the same for function template ? doesn't appear to be...
    // --> investigate and decide if we should mark it local nonetheless.
    // symbol.setLocal();
  }


  // j'imagine qu'au moment de récupérer le parent on peut savoir si le symbol est exporté
  // en regardant si l'un des parents est un ExportDecl.
  IndexerSymbol* parent_symbol = getParentSymbol(symbol, decl); 
  if (parent_symbol) {
    symbol.parentId = parent_symbol->id;
  }

  bool attr_const = false, attr_final = false, attr_override = false;

  for (const clang::Attr* attr : decl->attrs())
  {
    switch (attr->getKind())
    {
    case clang::attr::Const:
      attr_const = true;
      break;
    case clang::attr::Final:
      attr_final = true;
      break;
    case clang::attr::Override:
      attr_override = true;
      break;
    }
  }

  auto read_access = [&symbol, decl]() {
    clang::AccessSpecifier access = decl->getAccess();
    symbol.setFlag(SymbolFlag::Protected, (access == clang::AccessSpecifier::AS_protected));
    symbol.setFlag(SymbolFlag::Private, (access == clang::AccessSpecifier::AS_private));
  };

  auto read_fdecl_flags = [&symbol](const clang::FunctionDecl& fdecl) {
    symbol.setFlag(FunctionInfo::Delete, fdecl.isDeleted());
    symbol.setFlag(FunctionInfo::Static, fdecl.isStatic());
    symbol.setFlag(FunctionInfo::Constexpr, fdecl.isConstexpr());
    symbol.setFlag(FunctionInfo::Inline, fdecl.isInlined());
    symbol.setFlag(FunctionInfo::Noexcept, clang::isNoexceptExceptionSpec(fdecl.getExceptionSpecType()));
    };

  auto check_is_overloaded_operator = [&symbol](const clang::FunctionDecl& fdecl) {
    clang::OverloadedOperatorKind ook = fdecl.getOverloadedOperator();

    if (ook == clang::OverloadedOperatorKind::OO_None) return;

    symbol.kind = SymbolKind::Operator;
  };

  switch (decl->getKind())
  {
  case clang::Decl::Kind::CXXRecord:
  {
    read_access();

    auto* rdecl = llvm::dyn_cast<clang::RecordDecl>(decl);

    if (attr_final) {
      symbol.setFlag(ClassInfo::Final);
    }
    
    if (rdecl->isLambda()) {
      // clang currently does not define a specific "symbol kind" for 
      // lambdas and uses "class" instead* ; but the distinction seems
      // useful (at user level) so we overwrite any previously set value.
      // *indeed, a lambda is just syntactic sugar for a class. 
      symbol.kind = SymbolKind::Lambda;
    }
    
    // An empty name is not practical for printing, so we compute
    // a placeholder if none is present.
    // Note that lambdas have an empty name by default ;
    // i.e., getDeclSpelling() returns an empty string for such declarations
    // so it is expected that fillEmptyRecordName() will always be called
    // for a lambda.
    if (symbol.name.empty()) {
      fillEmptyRecordName(symbol, *rdecl);
    }
  }
  break;
  case clang::Decl::Kind::Enum:
  {
    read_access();

    const auto* enum_decl = llvm::dyn_cast<clang::EnumDecl>(decl);

    if (enum_decl->isScoped()) {
      symbol.kind = SymbolKind::EnumClass;
    }

    if (enum_decl->getIntegerTypeSourceInfo()) {
      const clang::QualType underlying_type = enum_decl->getIntegerType();
      symbol.getExtraInfo<EnumInfo>().underlyingType = prettyPrint(underlying_type, m_indexer);
    }
  }
  break;
  case clang::Decl::Kind::EnumConstant:
  {
    const auto* cst = llvm::dyn_cast<clang::EnumConstantDecl>(decl);

    symbol.getExtraInfo<EnumConstantInfo>().value = cst->getInitVal().getExtValue();

    if (cst->getInitExpr()) {
      symbol.getExtraInfo<EnumConstantInfo>().expression = prettyPrint(cst->getInitExpr(), m_indexer);
    }
  }
  break;
  case clang::Decl::Kind::Function:
  {
    auto* fdecl = llvm::dyn_cast<clang::FunctionDecl>(decl);
    read_fdecl_flags(*fdecl);
    check_is_overloaded_operator(*fdecl);

    symbol.getExtraInfo<FunctionInfo>().returnType = prettyPrint(fdecl->getReturnType(), m_indexer);
    symbol.getExtraInfo<FunctionInfo>().declaration = prettyPrint(*fdecl, m_indexer);
  }
  break;
  case clang::Decl::Kind::Field:
  {
    read_access();

    auto* fdecl = llvm::dyn_cast<clang::FieldDecl>(decl);
    symbol.setFlag(VariableInfo::Const, fdecl->getTypeSourceInfo()->getType().isConstQualified());

    auto& varinfo = symbol.getExtraInfo<VariableInfo>();

    varinfo.type = prettyPrint(fdecl->getTypeSourceInfo()->getType(), m_indexer);

    if (fdecl->getInClassInitializer()) {
      // TODO: do no write init unless variable is const ?
      varinfo.init = prettyPrint(fdecl->getInClassInitializer(), m_indexer);
    }
  }
  break;
  case clang::Decl::Kind::Var:
  {
    auto* vardecl = llvm::dyn_cast<clang::VarDecl>(decl);

    auto& varinfo = symbol.getExtraInfo<VariableInfo>();
    
    if (vardecl->isStaticDataMember()) {
      symbol.setFlag(VariableInfo::Static);

      if (vardecl->getInit()) {
        // TODO: do no write init unless variable is const ?
        varinfo.init = prettyPrint(vardecl->getInit(), m_indexer);
      }
    }

    symbol.setFlag(VariableInfo::Const, vardecl->getTypeSourceInfo()->getType().isConstQualified());

    varinfo.type = prettyPrint(vardecl->getTypeSourceInfo()->getType(), m_indexer);
  }
  break;
  case clang::Decl::Kind::CXXMethod:
  case clang::Decl::Kind::CXXConstructor:
  case clang::Decl::Kind::CXXDestructor:
  case clang::Decl::Kind::CXXConversion:
  {
    read_access();

    auto* mdecl = llvm::dyn_cast<clang::CXXMethodDecl>(decl);
    read_fdecl_flags(*mdecl);
    check_is_overloaded_operator(*mdecl);

    symbol.setFlag(FunctionInfo::Default, mdecl->isDefaulted());
    symbol.setFlag(FunctionInfo::Const, mdecl->isConst()); // TODO: or attr_const ?
    symbol.setFlag(FunctionInfo::Virtual, mdecl->isVirtual());
    symbol.setFlag(FunctionInfo::Pure, mdecl->isPureVirtual());

    // TODO: is this how it's supposed to be done ?
    symbol.setFlag(FunctionInfo::Final, attr_final);
    symbol.setFlag(FunctionInfo::Override, attr_override);

    if (symbol.kind == SymbolKind::Method || symbol.kind == SymbolKind::StaticMethod || symbol.kind == SymbolKind::Operator) {
      symbol.getExtraInfo<FunctionInfo>().returnType = prettyPrint(mdecl->getReturnType(), m_indexer);
    }
  }
  break;
  case clang::Decl::Kind::ParmVar:
  {
    auto* parmdecl = llvm::dyn_cast<clang::ParmVarDecl>(decl);
    symbol.setFlag(VariableInfo::Const, parmdecl->getTypeSourceInfo()->getType().isConstQualified());

    auto& info = symbol.getExtraInfo<ParameterInfo>();
    info.type = prettyPrint(parmdecl->getTypeSourceInfo()->getType(), m_indexer);
    info.parameterIndex = parmdecl->getFunctionScopeIndex(); 
    
    if (parmdecl->hasDefaultArg()) {
      info.defaultValue = prettyPrint(parmdecl->getDefaultArg(), m_indexer);
    }
  }
  break;
  case clang::Decl::Kind::TemplateTypeParm:
  {
    auto* parmdecl = llvm::dyn_cast<clang::TemplateTypeParmDecl>(decl);

    auto& info = symbol.getExtraInfo<ParameterInfo>();
    info.parameterIndex = parmdecl->getIndex();

    if (parmdecl->hasDefaultArgument()) {
      info.defaultValue = prettyPrint(parmdecl->getDefaultArgument(), m_indexer);
    }
  }
  break;
  case clang::Decl::Kind::NonTypeTemplateParm:
  {
    auto* parmdecl = llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(decl);

    auto& info = symbol.getExtraInfo<ParameterInfo>();
    info.parameterIndex = parmdecl->getIndex();

    if (parmdecl->getTypeSourceInfo()) {
      info.type = prettyPrint(parmdecl->getTypeSourceInfo()->getType(), m_indexer);
    }

    if (parmdecl->hasDefaultArgument()) {
      info.defaultValue = prettyPrint(parmdecl->getDefaultArgument(), m_indexer);
    }
  }
  break;
  case clang::Decl::Kind::Namespace:
  {
    auto* nsdecl = llvm::dyn_cast<clang::NamespaceDecl>(decl);
    if (nsdecl->isInline()) {
      symbol.kind = SymbolKind::InlineNamespace;
    }
  }
  break;
  case clang::Decl::Kind::NamespaceAlias:
  {
    auto* nsalias = llvm::dyn_cast<clang::NamespaceAliasDecl>(decl);

    auto& info = symbol.getExtraInfo<NamespaceAliasInfo>();

    if (auto* qual = nsalias->getQualifier()) {
      info.value = prettyPrint(*qual, m_indexer);
    }

    info.value += nsalias->getNamespace()->getName().str();
    // TODO: ideally, we would like to save the id of the target namespace, 
    // not just the string representation of it.
  }
  break;
  case clang::Decl::Kind::Import:
  {
    auto* imp = llvm::dyn_cast<clang::ImportDecl>(decl);

    // see clang::Sema::ActOnModuleDecl
    clang::TranslationUnitDecl* tu_decl = m_indexer.getAstContext()->getTranslationUnitDecl();
    clang::Module* m = tu_decl->getLocalOwningModule();
    m = m_indexer.getAstContext()->getCurrentNamedModule();
  }
  break;
  case clang::Decl::Kind::Export:
  {
    auto* expo = llvm::dyn_cast<clang::ExportDecl>(decl);
  }
  break;
  default:
    break;
  }
}

void SymbolCollector::fillSymbol(IndexerSymbol& symbol, const clang::IdentifierInfo* name, const clang::MacroInfo* macroInfo)
{
  symbol.name = name->getName().str();
  symbol.kind = SymbolKind::Macro;
}

IndexerSymbol* SymbolCollector::getParentSymbol(const IndexerSymbol& symbol, const clang::Decl* decl)
{
  if (!decl->getDeclContext()) {
    return nullptr;
  }

  const clang::DeclContext* ctx = decl->getDeclContext();

  while (ctx && ctx->getDeclKind() == clang::Decl::LinkageSpec) {
    ctx = ctx->getParent();
  }

  if (!ctx || ctx->isTranslationUnit()) {
    return nullptr;
  }

  auto* parent_decl = llvm::cast<clang::Decl>(ctx);

  if (!parent_decl) {
    return nullptr;
  }
  
  IndexerSymbol* parent_symbol = process(parent_decl); 
  return parent_symbol;
}

void IdxrDiagnosticConsumer::HandleDiagnostic(clang::DiagnosticsEngine::Level dlvl, const clang::Diagnostic& dinfo)
{
  clang::DiagnosticConsumer::HandleDiagnostic(dlvl, dinfo);

  Diagnostic d;

  d.level = static_cast<DiagnosticLevel>(dlvl);

  llvm::SmallString<1000> diag;
  dinfo.FormatDiagnostic(diag);
  d.message = diag.str().str();

  if (!m_indexer.initialized())
  {
    if (!dinfo.hasSourceManager())
    {
      std::cerr << "no source manager in HandleDiagnostic()" << std::endl;
    }

    return;
  }

  clang::SourceRange srcrange = dinfo.getLocation();
  clang::PresumedLoc ploc = m_indexer.getSourceManager().getPresumedLoc(srcrange.getBegin());

  if (!m_indexer.shouldIndexFile(ploc.getFileID())) {
    return;
  }

  d.fileID = m_indexer.getFileID(ploc.getFileID());

  if (!d.fileID) {
    return;
  }

  d.position = FilePosition(ploc.getLine(), ploc.getColumn());

  m_indexer.getCurrentIndex()->add(std::move(d));
}

void IdxrDiagnosticConsumer::finish()
{
  clang::DiagnosticConsumer::finish();
}

PreprocessingRecordIndexer::PreprocessingRecordIndexer(Indexer& idxr) :
  m_indexer(idxr)
{
  assert(m_indexer.getCurrentIndex());
}

void PreprocessingRecordIndexer::process(clang::PreprocessingRecord* ppRecord)
{
  if (!ppRecord) {
    return;
  }

  clang::SourceManager& sourceman = m_indexer.getAstContext()->getSourceManager();

  for (clang::PreprocessedEntity* ppe : *(ppRecord)) {
    switch (ppe->getKind())
    {
    case clang::PreprocessedEntity::InclusionDirectiveKind:
      process(*llvm::dyn_cast<clang::InclusionDirective>(ppe));
    break;
    case clang::PreprocessedEntity::MacroDefinitionKind:
      process(*llvm::dyn_cast<clang::MacroDefinitionRecord>(ppe));
      break;
    case clang::PreprocessedEntity::MacroExpansionKind:
      process(*llvm::dyn_cast<clang::MacroExpansion>(ppe));
      break;
    default:
      break;
    }
  }
}

clang::SourceManager& PreprocessingRecordIndexer::getSourceManager() const
{
  return m_indexer.getAstContext()->getSourceManager();
}

bool PreprocessingRecordIndexer::shouldIndexFile(const clang::FileID fileId) const
{
  return m_indexer.shouldIndexFile(fileId);
}

TranslationUnitIndex& PreprocessingRecordIndexer::currentIndex() const
{
  return *m_indexer.getCurrentIndex();
}

cppscanner::FileID PreprocessingRecordIndexer::idFile(const clang::FileID& fileId) const
{
  return m_indexer.getFileID(fileId);
}

cppscanner::FileID PreprocessingRecordIndexer::idFile(const std::string& filePath) const
{
  return m_indexer.fileIdentificator().getIdentification(filePath);
}

void PreprocessingRecordIndexer::process(clang::InclusionDirective& inclusionDirective)
{
  clang::SourceLocation range_begin = inclusionDirective.getSourceRange().getBegin();
  clang::FileID fileid = getSourceManager().getFileID(range_begin);

  if (!shouldIndexFile(fileid)) {
    return;
  }

  if (!inclusionDirective.getFile().has_value()) {
    return;
  }

  const clang::FileEntry& fileentry = inclusionDirective.getFile()->getFileEntry();
  llvm::StringRef realpath = fileentry.tryGetRealPathName();

  if (realpath.empty()) {
    return;
  }

  Include inc;
  inc.fileID = idFile(fileid);
  inc.line = getSourceManager().getSpellingLineNumber(range_begin);
  inc.includedFileID = idFile(realpath.str());

  currentIndex().add(inc);
}

void PreprocessingRecordIndexer::process(clang::MacroDefinitionRecord& mdr)
{
  // Since the preprocessing record is indexed after the translation unit
  // is parsed, we may have more information about a macro (e.g., whether 
  // it is used) then when it was collected.

  clang::Preprocessor* pp = m_indexer.getPreprocessor();
  const clang::MacroInfo* macro_info = pp->getMacroInfo(mdr.getName());

  if (!macro_info) {
    return;
  }

  // If everything worked fine, the macro definition should already have been 
  // handled by handleMacroOccurrence(), so we should be able to get its id
  // from the cache.
  SymbolID symid = m_indexer.symbolCollector().getMacroSymbolIdFromCache(macro_info);

  if (!symid) {
    return;
  }

  IndexerSymbol* symbol = m_indexer.getCurrentIndex()->getSymbolById(symid);

  if (!symbol) {
    return;
  }

  assert(symbol->kind == SymbolKind::Macro);

  symbol->setFlag(MacroInfo::MacroUsedAsHeaderGuard, macro_info->isUsedForHeaderGuard());
}

void PreprocessingRecordIndexer::process(clang::MacroExpansion& macroExpansion)
{
  // The MacroExpansion object contains information about where a macro 
  // was expanded while parsing the translation unit.
  // However, we can already collect that information in handleMacroOccurrence()
  // (which is called before the macro is expanded) and since clang does not appear 
  // to keep in memory the result of the macro expansion, 
  // there really is nothing more we can do here...
  (void)macroExpansion;
}

Indexer::Indexer(FileIndexingArbiter& fileIndexingArbiter, IndexingResultQueue& resultsQueue) :
  m_fileIndexingArbiter(fileIndexingArbiter),
  m_resultsQueue(resultsQueue),
  m_fileIdentificator(fileIndexingArbiter.fileIdentificator()),
  m_symbolCollector(*this),
  m_diagnosticConsumer(*this)
{

}

Indexer::~Indexer() = default;

FileIdentificator& Indexer::fileIdentificator()
{
  return m_fileIdentificator;
}

SymbolCollector& Indexer::symbolCollector()
{
  return m_symbolCollector;
}

clang::DiagnosticConsumer* Indexer::getDiagnosticConsumer()
{
  return &m_diagnosticConsumer;
}

TranslationUnitIndex* Indexer::getCurrentIndex() const
{
  return m_index.get();
}

clang::SourceManager& Indexer::getSourceManager() const
{
  if (!mAstContext) {
    throw std::runtime_error("getSourceManager() called but no ast context");
  }

  return mAstContext->getSourceManager();
}

bool Indexer::shouldIndexFile(clang::FileID fileId)
{
  auto it = m_ShouldIndexFileCache.find(fileId);

  if (it != m_ShouldIndexFileCache.end()) {
    return it->second;
  }

  cppscanner::FileID fid = getFileID(fileId);
  bool ok = m_fileIndexingArbiter.shouldIndex(fid, m_index.get());
  m_ShouldIndexFileCache[fileId] = ok;

  if (ok) {
    getCurrentIndex()->indexedFiles.insert(fid);
  }

  return ok;
}

bool Indexer::ShouldTraverseDecl(const clang::Decl* decl)
{
  clang::SourceManager& source_manager = getSourceManager();
  clang::SourceLocation loc = decl->getLocation();
  clang::FileID file_id = source_manager.getFileID(loc);
  return shouldIndexFile(file_id);
}

cppscanner::FileID Indexer::getFileID(clang::FileID clangFileId)
{
  auto it = m_FileIdCache.find(clangFileId);

  if (it != m_FileIdCache.end()) {
    return it->second;
  }

  const clang::FileEntry* fileentry = clangFileId.isValid() ?
    getSourceManager().getFileEntryForID(clangFileId) : nullptr;

  if (!fileentry) {
    m_FileIdCache[clangFileId] = invalidFileID();
    return {};
  }

  llvm::StringRef pathref = fileentry->tryGetRealPathName();

  cppscanner::FileID fid = m_fileIdentificator.getIdentification(pathref.str());
  m_FileIdCache[clangFileId] = fid;
  return fid;
}

clang::ASTContext* Indexer::getAstContext() const
{
  return mAstContext;
}

clang::Preprocessor* Indexer::getPreprocessor() const
{
  return m_pp.get();
}

bool Indexer::initialized() const
{
  return mAstContext != nullptr && m_index != nullptr;
}

void Indexer::initialize(clang::ASTContext& Ctx)
{
  mAstContext = &Ctx;
  m_FileIdCache.clear();
  m_ShouldIndexFileCache.clear();
  m_symbolCollector.reset();

  m_index = std::make_unique<TranslationUnitIndex>();
  m_index->fileIdentificator = &m_fileIdentificator;
}

void Indexer::setPreprocessor(std::shared_ptr<clang::Preprocessor> pp)
{
  m_pp = pp;

  // note: we need to wait until after the translation unit was indexed
  // to get a non-empty preprocessing record.
  // therefore indexPreprocessingRecord() is called in finish() and not
  // here!
  /*if (pp && pp->getPreprocessingRecord()) {
    indexPreprocessingRecord(*pp);
  }*/
}

bool Indexer::handleDeclOccurrence(const clang::Decl* decl, clang::index::SymbolRoleSet roles,
  llvm::ArrayRef<clang::index::SymbolRelation> relations,
  clang::SourceLocation loc, ASTNodeInfo astNode)
{
  if (!shouldIndexFile(getSourceManager().getFileID(loc))) {
    return true;
  }

  IndexerSymbol* symbol = m_symbolCollector.process(decl);

  if (!symbol)
    return true;

  int line = getSourceManager().getSpellingLineNumber(loc);
  int col = getSourceManager().getSpellingColumnNumber(loc);

  SymbolReference symref;
  symref.fileID = getFileID(getSourceManager().getFileID(loc));
  symref.symbolID = symbol->id;
  symref.position = FilePosition(line, col);

  if (astNode.Parent) {
    if (auto* fndecl = llvm::dyn_cast<clang::FunctionDecl>(astNode.Parent)) {
      IndexerSymbol* function_symbol = m_symbolCollector.process(fndecl);
      if (function_symbol) {
        symref.referencedBySymbolID = function_symbol->id;
      }
    }
  }

  symref.flags = 0;
  symref.flags |= (roles & (int)clang::index::SymbolRole::Definition) ? SymbolReference::Definition : 0;
  symref.flags |= (roles & (int)clang::index::SymbolRole::Declaration) ? SymbolReference::Declaration : 0;
  symref.flags |= (roles & (int)clang::index::SymbolRole::Reference) ? SymbolReference::Reference : 0; // useful ?
  symref.flags |= (roles & (int)clang::index::SymbolRole::Implicit) ? SymbolReference::Implicit : 0;
  symref.flags |= (roles & (int)clang::index::SymbolRole::Read) ? SymbolReference::Read : 0;
  symref.flags |= (roles & (int)clang::index::SymbolRole::Write) ? SymbolReference::Write : 0;
  symref.flags |= (roles & (int)clang::index::SymbolRole::Call) ? SymbolReference::Call : 0;
  symref.flags |= (roles & (int)clang::index::SymbolRole::Dynamic) ? SymbolReference::Dynamic : 0; // what is it?
  symref.flags |= (roles & (int)clang::index::SymbolRole::AddressOf) ? SymbolReference::AddressOf : 0;

  getCurrentIndex()->add(symref);

  processRelations(std::pair(decl, symbol), loc, relations);

  return true;
}

bool Indexer::handleMacroOccurrence(const clang::IdentifierInfo* name,
  const clang::MacroInfo* macroInfo, clang::index::SymbolRoleSet roles,
  clang::SourceLocation loc) 
{
  if (!shouldIndexFile(getSourceManager().getFileID(loc))) {
    return true;
  }

  IndexerSymbol* symbol = m_symbolCollector.process(name, macroInfo, loc);

  if (!symbol)
    return true;

  if (roles & (int)clang::index::SymbolRole::Definition) {
    if (!macroInfo->tokens_empty()) {
      const clang::Token& first_token = macroInfo->tokens().front();
      const clang::Token& last_token = macroInfo->tokens().back();
      auto range = clang::CharSourceRange::getCharRange(first_token.getLocation(), last_token.getEndLoc());
      symbol->getExtraInfo<MacroInfo>().definition = clang::Lexer::getSourceText(range, getSourceManager(), getAstContext()->getLangOpts()).str();
    }
  }

  int line = getSourceManager().getSpellingLineNumber(loc);
  int col = getSourceManager().getSpellingColumnNumber(loc);

  SymbolReference symref;
  symref.fileID = getFileID(getSourceManager().getFileID(loc));
  symref.symbolID = symbol->id;
  symref.position = FilePosition(line, col);

  symref.flags = 0;
  symref.flags |= (roles & (int)clang::index::SymbolRole::Definition) ? SymbolReference::Definition : 0;
  symref.flags |= (roles & (int)clang::index::SymbolRole::Declaration) ? SymbolReference::Declaration : 0;
  symref.flags |= (roles & (int)clang::index::SymbolRole::Reference) ? SymbolReference::Reference : 0;

  if (roles & (int)clang::index::SymbolRole::Reference) {
    // note about recording the result of expanding the macro:
    // Looking at Preprocessor::HandleMacroExpandedIdentifier(), it appears the macro 
    // is actually expanded after this function is called ; meaning we have no way
    // to easily get information about the expansion from clang at this point.
    // The Woboq codebrowser expands the macro manually in PreprocessorCallback::MacroExpands()
    // by creating its own clang::Lexer which is honestly too much of a pain.
  }

  getCurrentIndex()->add(symref);

  return true;
}

bool Indexer::handleModuleOccurrence(const clang::ImportDecl *ImportD,
  const clang::Module *Mod, clang::index::SymbolRoleSet Roles,
  clang::SourceLocation Loc) 
{
  // TODO: handle module occurrence
  return true;
}

void Indexer::finish()
{
  if (m_pp && m_pp->getPreprocessingRecord()) {
    indexPreprocessingRecord(*m_pp);
  }

  // if we want to post-process diagnostics, we may use:
  clang::DiagnosticsEngine& de = mAstContext->getDiagnostics();
  clang::DiagnosticConsumer* dc = de.getClient();
  (void)dc;

  m_resultsQueue.write(std::move(*m_index));
  m_index.reset();

  mAstContext = nullptr;
}

namespace
{

std::optional<RelationKind> getIndexableRelation(clang::index::SymbolRoleSet srs)
{
  if (srs & (int)clang::index::SymbolRole::RelationBaseOf) {
    return RelationKind::BaseOf;
  } else if (srs & (int)clang::index::SymbolRole::RelationOverrideOf) {
    return RelationKind::OverriddenBy;
  } 

  return std::nullopt;
}

AccessSpecifier tr(const clang::AccessSpecifier access)
{
  switch (access)
  {
  case clang::AS_public: return AccessSpecifier::Public;
  case clang::AS_protected: return AccessSpecifier::Protected;
  case clang::AS_private: return AccessSpecifier::Private;
  case clang::AS_none: [[fallthrough]]; default: return AccessSpecifier::Invalid;
  }
}

} // namespace

void Indexer::processRelations(std::pair<const clang::Decl*, IndexerSymbol*> declAndSymbol, clang::SourceLocation refLocation, llvm::ArrayRef<clang::index::SymbolRelation> relations)
{
  for (const clang::index::SymbolRelation& rel : relations)
  {
    std::optional<RelationKind> optRK = getIndexableRelation(rel.Roles);

    if (!optRK) {
      continue;
    }

    RelationKind rk = *optRK;

    switch (rk)
    {
    case RelationKind::BaseOf:
    {
      IndexerSymbol* derived = m_symbolCollector.process(rel.RelatedSymbol);

      if (!derived)
        break;

      BaseOf bof;
      bof.baseClassID = declAndSymbol.second->id;
      bof.derivedClassID = derived->id;
      bof.accessSpecifier = AccessSpecifier::Invalid;

      auto* class_decl = llvm::dyn_cast<clang::CXXRecordDecl>(rel.RelatedSymbol);

      if (class_decl) {
        for (const clang::CXXBaseSpecifier& base : class_decl->bases()) {
          if (base.getSourceRange().fullyContains(refLocation)) {
            bof.accessSpecifier = tr(base.getAccessSpecifier());
            break;
          }
        }
      }

      getCurrentIndex()->add(bof);
    }
    break;
    case RelationKind::OverriddenBy:
    {
      IndexerSymbol* base_function = m_symbolCollector.process(rel.RelatedSymbol);

      if (!base_function)
        break;

      Override ov;
      ov.baseMethodID = base_function->id;
      ov.overrideMethodID = declAndSymbol.second->id;

      getCurrentIndex()->add(ov);
    }
    break;
    default:
      break;
    }
  }
}

void Indexer::indexPreprocessingRecord(clang::Preprocessor& pp)
{
  PreprocessingRecordIndexer pprindexer{ *this };
  pprindexer.process(pp.getPreprocessingRecord());
}

} // namespace cppscanner

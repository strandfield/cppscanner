// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#include "indexer.h"

#include "fileidentificator.h"

#include "cppscanner/indexer/astvisitor.h"
#include "cppscanner/indexer/fileindexingarbiter.h"
#include "cppscanner/index/symbolid.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/ExprCXX.h>
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

/**
* \brief a class for collecting C++ symbols while indexing a translation unit
* 
* This class creates an IndexerSymbol for each C++ symbol encountered while 
* indexing a translation unit. Macros are also handled by this class.
* 
* Because all declarations go through this class, it is also used by the 
* Indexer class for post-processing purposes: the location of some declarations 
* that went thought the SymbolCollector are recorded in the output TranslationUnitIndex.
*/
class SymbolCollector
{
private:
  Indexer& m_indexer;
  std::map<const clang::Decl*, SymbolID> m_symbolIdCache;
  std::map<const clang::MacroInfo*, SymbolID> m_macroIdCache;
  std::map<const clang::Module*, SymbolID> m_moduleIdCache;

public:
  explicit SymbolCollector(Indexer& idxr);

  void reset();

  IndexerSymbol* process(const clang::Decl* decl);
  IndexerSymbol* process(const clang::IdentifierInfo* name, const clang::MacroInfo* macroInfo);
  IndexerSymbol* process(const clang::Module* moduleInfo);

  SymbolID getMacroSymbolIdFromCache(const clang::MacroInfo* macroInfo) const;

  const std::map<const clang::Decl*, SymbolID>& declarations() const;

protected:
  std::string getDeclSpelling(const clang::Decl* decl);
  void fillSymbol(IndexerSymbol& symbol, const clang::Decl* decl);
  void fillSymbol(IndexerSymbol& symbol,const clang::IdentifierInfo* name, const clang::MacroInfo* macroInfo);
  void fillSymbol(IndexerSymbol& symbol, const clang::Module* moduleInfo);
  IndexerSymbol* getParentSymbol(const IndexerSymbol& symbol, const clang::Decl* decl);
};

/**
* \brief class for collecting information from a PreprocessingRecord
* 
* This class is used by the Indexer class for listing files that were #included 
* in the translation unit and for checking if macros were used as header guards.
*/
class PreprocessingRecordIndexer
{
private:
  Indexer& m_indexer;
public:
  explicit PreprocessingRecordIndexer(Indexer& idxr);

  void process(clang::PreprocessingRecord* ppRecord);

protected:
  clang::SourceManager& getSourceManager() const;
  bool shouldIndexFile(const clang::FileID fileId) const;
  TranslationUnitIndex& currentIndex() const;
  cppscanner::FileID idFile(const clang::FileID& fileId) const;
  cppscanner::FileID idFile(const std::string& filePath) const;

protected:
  void process(clang::InclusionDirective& inclusionDirective);
  void process(clang::MacroDefinitionRecord& mdr);
  void process(clang::MacroExpansion& macroExpansion);
};

} // namespace cppscanner

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

std::string getUSR(const clang::IdentifierInfo* name, const clang::MacroInfo* macroInfo, const clang::SourceManager& sourceManager)
{
  llvm::SmallVector<char> chars;

  bool ignore = clang::index::generateUSRForMacro(name->getName(), macroInfo->getDefinitionLoc(), sourceManager, chars);

  if (ignore) {
    throw std::runtime_error("could not generate usr");
  }

  return std::string(chars.data(), chars.size());
}

std::string getUSR(const clang::Module* moduleInfo)
{
  llvm::SmallVector<char> chars;
  llvm::raw_svector_ostream stream{ chars };

  bool ignore = clang::index::generateFullUSRForModule(moduleInfo, stream);

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

std::string prettyPrint(const clang::TypeSourceInfo* info, const Indexer& idxr)
{
  if (!info) {
    return {};
  } 

  return prettyPrint(info->getType(), idxr);
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

void fillEmptyName(IndexerSymbol& symbol, const clang::Decl& decl)
{
  (void)decl; // we are not going to use 'decl' for now

  switch (symbol.kind)
  {
  case SymbolKind::Namespace:
    symbol.name = "__anonymous_namespace_" + symbol.id.toHex();
    break;
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
  case SymbolKind::Enum:
  case SymbolKind::EnumClass:
    symbol.name = "__anonymous_enum_" + symbol.id.toHex();
    break;
  default:
    break;
  }
}
void fillEmptyRecordName(IndexerSymbol& symbol, const clang::RecordDecl& decl)
{
  fillEmptyName(symbol, decl);
}

SymbolCollector::SymbolCollector(Indexer& idxr) : 
  m_indexer(idxr)
{

}

void SymbolCollector::reset()
{
  m_symbolIdCache.clear();
  m_macroIdCache.clear();
  m_moduleIdCache.clear();
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

IndexerSymbol* SymbolCollector::process(const clang::IdentifierInfo* name, const clang::MacroInfo* macroInfo)
{
  auto [it, inserted] = m_macroIdCache.try_emplace(macroInfo, SymbolID());

  if (inserted) {
    try {
      std::string usr = getUSR(name, macroInfo, m_indexer.getAstContext()->getSourceManager());
      it->second = computeSymbolIdFromUsr(usr);
    } catch (const std::exception&) {
      m_macroIdCache.erase(it);
      return nullptr;
    }
  };

  SymbolID symid = it->second;
  IndexerSymbol& symbol = m_indexer.getCurrentIndex()->symbols[symid];

  if (inserted) {
    symbol.id = symid;
    fillSymbol(symbol, name, macroInfo);
  } else {
    assert(symbol.id != SymbolID());
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

IndexerSymbol* SymbolCollector::process(const clang::Module* moduleInfo)
{
  auto [it, inserted] = m_moduleIdCache.try_emplace(moduleInfo, SymbolID());

  if (inserted) {
    try {
      std::string usr = getUSR(moduleInfo);
      it->second = computeSymbolIdFromUsr(usr);
    } catch (const std::exception&) {
      m_moduleIdCache.erase(it);
      return nullptr;
    }
  };

  SymbolID symid = it->second;
  IndexerSymbol& symbol = m_indexer.getCurrentIndex()->symbols[symid];

  if (inserted) {
    symbol.id = symid;
    fillSymbol(symbol, moduleInfo);
  } else {
    assert(symbol.id != SymbolID());
  }

  return &symbol;
}

SymbolID SymbolCollector::getMacroSymbolIdFromCache(const clang::MacroInfo* macroInfo) const
{
  auto it = m_macroIdCache.find(macroInfo);
  return it != m_macroIdCache.end() ? it->second : SymbolID();
}

const std::map<const clang::Decl*, SymbolID>& SymbolCollector::declarations() const
{
  return m_symbolIdCache;
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

  if (symbol.kind == SymbolKind::Unknown) 
  {
    if (llvm::dyn_cast<const clang::LabelDecl>(decl)) {
      symbol.kind = SymbolKind::GotoLabel;
    } else if (llvm::dyn_cast<const clang::CXXDeductionGuideDecl>(decl)) {
      symbol.kind = SymbolKind::DeductionGuide;
    }
  }

  // This assert is left here so that we are notified when unknown symbols
  // are encountered when building in debug mode.
  // In release mode, the assert goes away and this isn't an issue. Unknown
  // symbols are allowed in a snapshot.
  assert(symbol.kind != SymbolKind::Unknown);

  if (const auto* fun = llvm::dyn_cast<clang::FunctionDecl>(decl)) 
  {
    symbol.name = computeName(*fun, m_indexer);
  }

  if (info.Properties & (clang::index::SymbolPropertySet)clang::index::SymbolProperty::Local) {
    symbol.setLocal();
  }

  if (symbol.kind == SymbolKind::TemplateTypeParameter 
    || symbol.kind == SymbolKind::NonTypeTemplateParameter 
    || symbol.kind == SymbolKind::TemplateTemplateParameter) {
    // note: clang's handling of the 'local' flag for template parameters does not seem to 
    // be very consistent.
    // it seems to be set for TemplateTypeParameter for function template but not class template,
    // and to not be set for NonTypeTemplateParameter.
    // there may very well be a valid reason for that, but for now we will just consider
    // all template parameters to be local symbols.
    symbol.setLocal();
  }


  // j'imagine qu'au moment de r�cup�rer le parent on peut savoir si le symbol est export�
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

    varinfo.type = prettyPrint(fdecl->getTypeSourceInfo(), m_indexer);

    if (fdecl->getInClassInitializer() && (symbol.flags & (VariableInfo::Const | VariableInfo::Constexpr))) {
      varinfo.init = prettyPrint(fdecl->getInClassInitializer(), m_indexer);
    }
  }
  break;
  case clang::Decl::Kind::Var:
  {
    auto* vardecl = llvm::dyn_cast<clang::VarDecl>(decl);
    auto& varinfo = symbol.getExtraInfo<VariableInfo>();

    symbol.setFlag(VariableInfo::Const, vardecl->getTypeSourceInfo()->getType().isConstQualified());
    symbol.setFlag(VariableInfo::Constexpr, vardecl->isConstexpr());
    symbol.setFlag(VariableInfo::Static, vardecl->isStaticDataMember());

    varinfo.type = prettyPrint(vardecl->getTypeSourceInfo(), m_indexer);

    if (vardecl->getInit() && (symbol.flags & (VariableInfo::Const | VariableInfo::Constexpr))) {
      varinfo.init = prettyPrint(vardecl->getInit(), m_indexer);
    }
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
      if (!parmdecl->hasUninstantiatedDefaultArg()) {
        info.defaultValue = prettyPrint(parmdecl->getDefaultArg(), m_indexer);
      }
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
      info.type = prettyPrint(parmdecl->getTypeSourceInfo(), m_indexer);
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
  case clang::Decl::Kind::Typedef:
  {
    auto* tpd_decl = llvm::dyn_cast<clang::TypedefDecl>(decl);
    // it seems clang uses the "typealias" kind for typedefs.
    // we use a dedicated kind instead.
    assert(symbol.kind == SymbolKind::TypeAlias);
    symbol.kind = SymbolKind::Typedef;

    // TODO: handle "typedef enum {} my_enum;"
    // --> rename the enum ?
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

  if (symbol.name.empty()) {
    fillEmptyName(symbol, *decl);
  }
}

void SymbolCollector::fillSymbol(IndexerSymbol& symbol, const clang::IdentifierInfo* name, const clang::MacroInfo* macroInfo)
{
  symbol.name = name->getName().str();
  symbol.kind = SymbolKind::Macro;
}

void SymbolCollector::fillSymbol(IndexerSymbol& symbol, const clang::Module* moduleInfo)
{
  symbol.kind = SymbolKind::Module;
  symbol.name = moduleInfo->Name;
}

IndexerSymbol* SymbolCollector::getParentSymbol(const IndexerSymbol& symbol, const clang::Decl* decl)
{
  if (!decl->getDeclContext()) {
    return nullptr;
  }

  const clang::DeclContext* ctx = decl->getDeclContext();

  auto skip_decl = [](const clang::DeclContext& decl) {
    return decl.getDeclKind() == clang::Decl::LinkageSpec
      || decl.getDeclKind() == clang::Decl::Export;
    };

  while (ctx && skip_decl(*ctx)) {
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

// a class for receiving diagnostics from clang.
// the diagnostics are printed to cout and forwarded to the Indexer.
// an instance of this class is created and managed by the Indexer
// (in particular, its lifetime is within the lifetime of the Indexer)
class IdxrDiagnosticConsumer : public clang::DiagnosticConsumer
{
  Indexer& m_indexer;

public:
  explicit IdxrDiagnosticConsumer(Indexer& idxr) : 
    m_indexer(idxr)
  {
  }

  bool IncludeInDiagnosticCounts() const final { return false; }

  void HandleDiagnostic(clang::DiagnosticsEngine::Level dlvl, const clang::Diagnostic& dinfo) final
  {
    clang::DiagnosticConsumer::HandleDiagnostic(dlvl, dinfo);

    if (!m_indexer.initialized())
    {
      if (!dinfo.hasSourceManager())
      {
        std::cerr << "no source manager in HandleDiagnostic()" << std::endl;
        return;
      }

      const auto level = static_cast<DiagnosticLevel>(dlvl);

      const clang::SourceRange srcrange = dinfo.getLocation();
      clang::PresumedLoc ploc = dinfo.getSourceManager().getPresumedLoc(srcrange.getBegin());

      llvm::SmallString<1000> diag;
      dinfo.FormatDiagnostic(diag);
      std::string message = diag.str().str();

      if (ploc.isValid())
      {
        std::cout << ploc.getLine() << ":" << ploc.getColumn() << ": " 
          << getDiagnosticLevelString(level) << ": " << message << std::endl;
      }
      else
      {
        std::cout << getDiagnosticLevelString(level) << ": " << message << std::endl;
      }

      return;
    }

    m_indexer.HandleDiagnostic(dlvl, dinfo);
  }

  void finish() final
  {
    clang::DiagnosticConsumer::finish();
  }

  void printDiagnostic(const Diagnostic& d, const std::string& filePath)
  {
    std::cout << filePath << ":"
      << d.position.line() << ":" << d.position.column() << ": "
      << getDiagnosticLevelString(d.level) << ": " << d.message << std::endl;
  }
};

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
  SymbolID symid = m_indexer.getMacroSymbolIdFromCache(macro_info);

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

ForwardingIndexDataConsumer::ForwardingIndexDataConsumer(Indexer* indexer)
  : m_indexer(indexer)
{

}

Indexer& ForwardingIndexDataConsumer::indexer() const
{
  assert(m_indexer);
  return *m_indexer;
}

void ForwardingIndexDataConsumer::setIndexer(Indexer& indexer)
{
  m_indexer = &indexer;
}

void ForwardingIndexDataConsumer::initialize(clang::ASTContext& Ctx)
{
  indexer().initialize(Ctx);
}

void ForwardingIndexDataConsumer::setPreprocessor(std::shared_ptr<clang::Preprocessor> PP)
{
  indexer().setPreprocessor(PP);
}

bool ForwardingIndexDataConsumer::handleDeclOccurrence(const clang::Decl* D, clang::index::SymbolRoleSet Roles,
  llvm::ArrayRef<clang::index::SymbolRelation> relations,
  clang::SourceLocation Loc, ASTNodeInfo ASTNode)
{
  return indexer().handleDeclOccurrence(D, Roles, relations, Loc, ASTNode);
}

bool ForwardingIndexDataConsumer::handleMacroOccurrence(const clang::IdentifierInfo* Name,
  const clang::MacroInfo* MI, clang::index::SymbolRoleSet Roles,
  clang::SourceLocation Loc)
{
  return indexer().handleMacroOccurrence(Name, MI, Roles, Loc);
}

bool ForwardingIndexDataConsumer::handleModuleOccurrence(const clang::ImportDecl* ImportD,
  const clang::Module* Mod, clang::index::SymbolRoleSet Roles,
  clang::SourceLocation Loc)
{
  return indexer().handleModuleOccurrence(ImportD, Mod, Roles, Loc);
}

void ForwardingIndexDataConsumer::finish() 
{
  indexer().finish();
}


Indexer::Indexer(FileIndexingArbiter& fileIndexingArbiter) :
  m_fileIndexingArbiter(fileIndexingArbiter),
  m_fileIdentificator(fileIndexingArbiter.fileIdentificator()),
  m_symbolCollector(std::make_unique<SymbolCollector>(*this))
{

}

Indexer::~Indexer() = default;

FileIdentificator& Indexer::fileIdentificator()
{
  return m_fileIdentificator;
}

SymbolCollector& Indexer::symbolCollector() const
{
  return *m_symbolCollector;
}

clang::DiagnosticConsumer* Indexer::getOrCreateDiagnosticConsumer()
{
  if (!m_diagnosticConsumer)
  {
    m_diagnosticConsumer = std::make_unique<IdxrDiagnosticConsumer>(*this);
  }

  return m_diagnosticConsumer.get();
}

TranslationUnitIndex* Indexer::getCurrentIndex() const
{
  return m_index.get();
}

void Indexer::resetCurrentIndex()
{
  m_index.reset();
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
  bool ok = m_fileIndexingArbiter.shouldIndex(fid, getCurrentIndex());
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

std::pair<FileID, FilePosition> Indexer::convert(const clang::SourceLocation& loc)
{
  FileID f = getFileID(getSourceManager().getFileID(loc));
  int line = getSourceManager().getSpellingLineNumber(loc);
  int col = getSourceManager().getSpellingColumnNumber(loc);
  return std::pair(f, FilePosition(line, col));
}

std::pair<FileID, FilePosition> Indexer::convert(clang::FileID fileId, const clang::SourceLocation& loc)
{
  FileID f = getFileID(fileId);
  int line = getSourceManager().getSpellingLineNumber(loc);
  int col = getSourceManager().getSpellingColumnNumber(loc);
  return std::pair(f, FilePosition(line, col));
}

clang::FileID Indexer::getClangFileID(const cppscanner::FileID id)
{
  for (const auto& p : m_FileIdCache)
  {
    if (p.second == id) {
      return p.first;
    }
  }

  return {};
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
  symbolCollector().reset();

  m_index = std::make_unique<TranslationUnitIndex>();
  m_index->mainFileId = getFileID(Ctx.getSourceManager().getMainFileID());
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
  clang::SourceLocation loc, clang::index::IndexDataConsumer::ASTNodeInfo astNode)
{
  if (!shouldIndexFile(getSourceManager().getFileID(loc))) {
    return true;
  }

  IndexerSymbol* symbol = symbolCollector().process(decl);

  if (!symbol)
    return true;

  SymbolReference symref;
  std::tie(symref.fileID, symref.position) = convert(loc);
  symref.symbolID = symbol->id;

  if (astNode.Parent) {
    if (auto* fndecl = llvm::dyn_cast<clang::FunctionDecl>(astNode.Parent)) {
      IndexerSymbol* function_symbol = symbolCollector().process(fndecl);
      if (function_symbol) {
        symref.referencedBySymbolID = function_symbol->id;
      }
    }
  }

  // note: clang defines a Reference role, but it is set when none of Declaration
  // or Definition is set, making it redundant.
  // see IndexingContext::handleDeclOccurrence() for the details.
  symref.flags = 0;
  symref.flags |= (roles & (int)clang::index::SymbolRole::Definition) ? SymbolReference::Definition : 0;
  symref.flags |= (roles & (int)clang::index::SymbolRole::Declaration) ? SymbolReference::Declaration : 0;
  symref.flags |= (roles & (int)clang::index::SymbolRole::Implicit) ? SymbolReference::Implicit : 0;
  symref.flags |= (roles & (int)clang::index::SymbolRole::Read) ? SymbolReference::Read : 0;
  symref.flags |= (roles & (int)clang::index::SymbolRole::Write) ? SymbolReference::Write : 0;
  symref.flags |= (roles & (int)clang::index::SymbolRole::Call) ? SymbolReference::Call : 0;
  symref.flags |= (roles & (int)clang::index::SymbolRole::Dynamic) ? SymbolReference::Dynamic : 0;
  symref.flags |= (roles & (int)clang::index::SymbolRole::AddressOf) ? SymbolReference::AddressOf : 0;

  if (symref.flags & (SymbolReference::Declaration | SymbolReference::Definition)) {
    if (!symbol->testFlag(SymbolFlag::FromProject)) {
      symbol->setFlag(SymbolFlag::FromProject);
    }

    if (decl->isImplicit()) {
      symref.flags |= SymbolReference::Implicit;
    }
  }

  if (astNode.OrigE) {
    // std::cout << astNode.OrigE->getStmtClassName() << " of " << symbol->name << " @" << line << ":" << col << std::endl;

    if (auto* declrefexpr = llvm::dyn_cast<clang::DeclRefExpr>(astNode.OrigE)) {
      // can we do something here ?
    } else if (auto* memexpr = llvm::dyn_cast<clang::MemberExpr>(astNode.OrigE)) {
      if (memexpr->getMemberLoc().isInvalid()) {
        // the name of the referenced member isn't written.
        // should always happen with conversion function
        symref.flags |= SymbolReference::Implicit;
      }
    } else if (auto* ctorexpr = llvm::dyn_cast<clang::CXXConstructExpr>(astNode.OrigE)) {

      // as of llvm version 18, clang does not seem to set the SymbolRole::Implicit flag
      // when a constructor is (implicitly) called with a brace-init list.
      // e.g. manhattanLength({x,y})
      if (ctorexpr->getParenOrBraceRange().getBegin() == ctorexpr->getSourceRange().getBegin()) {
        // the name of the constructor isn't written
        symref.flags |= SymbolReference::Implicit;

        (void)ctorexpr->isListInitialization(); // if we ever want to distinguish {} from ()
      }

      if (ctorexpr->getParenOrBraceRange().getBegin().isInvalid()) {
        // there are no parentheses (or braces), meaning that we are implicitly using
        // a conversion constructor.
        symref.flags |= SymbolReference::Implicit;
      }
    }
  }

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

  IndexerSymbol* symbol = symbolCollector().process(name, macroInfo);

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

  if (roles & (int)clang::index::SymbolRole::Reference) {
    // note about recording the result of expanding the macro:
    // Looking at Preprocessor::HandleMacroExpandedIdentifier(), it appears the macro 
    // is actually expanded after this function is called ; meaning we have no way
    // to easily get information about the expansion from clang at this point.
    // The Woboq codebrowser expands the macro manually in PreprocessorCallback::MacroExpands()
    // by creating its own clang::Lexer which is honestly too much of a pain.
  }

  if (symref.flags & (SymbolReference::Declaration | SymbolReference::Definition)) {
    if (!symbol->testFlag(SymbolFlag::FromProject)) {
      symbol->setFlag(SymbolFlag::FromProject);
    }
  }

  getCurrentIndex()->add(symref);

  return true;
}

bool Indexer::handleModuleOccurrence(const clang::ImportDecl *importD,
  const clang::Module *moduleInfo, clang::index::SymbolRoleSet roles,
  clang::SourceLocation loc) 
{
  if (!shouldIndexFile(getSourceManager().getFileID(loc))) {
    return true;
  }

  IndexerSymbol* symbol = symbolCollector().process(moduleInfo);

  if (!symbol)
    return true;

  int line = getSourceManager().getSpellingLineNumber(loc);
  int col = getSourceManager().getSpellingColumnNumber(loc);

  SymbolReference symref;
  symref.fileID = getFileID(getSourceManager().getFileID(loc));
  symref.symbolID = symbol->id;
  symref.position = FilePosition(line, col);

  getCurrentIndex()->add(symref);

  return true;
}

static void markImplicitReferences(TranslationUnitIndex& index, std::vector<SymbolReference>::iterator begin, std::vector<SymbolReference>::iterator end, Indexer& indexer)
{
  // We have multiple symbol references at the same location.
  // We want to mark all of them as "implicit" except one that will be the "primary" 
  // symbol reference at the location.
  // 
  // How can such situation arise?
  // - when declaring a partial class template specialization, the name 
  //   of the primary template is referenced when defining the specifialization
  //   (both the specialization and primary template have the same name);
  // - when a constructor is explicitly called, the name of the class is 
  //   also referenced;
  // - when an object member is initialized in the member-init-list of a constructor,
  //   the constructor of that member may be referenced at the same loc as 
  //   the field;
  // - other cases ?

  // filter references that are already implicit
  end = std::partition(begin, end, [](const SymbolReference& ref) {
    return !testFlag(ref, SymbolReference::Implicit);
    });

  size_t n = std::distance(begin, end);

  if (n <= 1) {
    return;
  }

  size_t ndef = std::count_if(begin, end, [](const SymbolReference& ref) -> bool {
    return ref.flags & SymbolReference::Definition;
    });

  if (ndef == 1) 
  {
    // multiple symbols are referenced at the same location but only one 
    // of the reference is a definition.
    // we make it take precedence over the others.
    std::for_each(begin, end, [](SymbolReference& ref) {
      if (!(ref.flags & SymbolReference::Definition)) {
        ref.flags |= SymbolReference::Implicit;
      }
      });

    return;
  } 
  else if (ndef == 0)
  {
    // note: the following does not work that well with a using declaration.
    // because the decl is the "using" and is in a way implicit,
    // but the referenced constructor is not referenced implicitly

    size_t ndecl = std::count_if(begin, end, [](const SymbolReference& ref) -> bool {
      return ref.flags & SymbolReference::Declaration;
      });

    if (ndecl == 1) 
    {
      std::for_each(begin, end, [](SymbolReference& ref) {
        if (!(ref.flags & SymbolReference::Declaration)) {
          ref.flags |= SymbolReference::Implicit;
        }
        });

      return;
    }
  }

  if (n == 2)
  {
    size_t nctor = std::count_if(begin, end, [&index](const SymbolReference& ref) -> bool {
      return index.getSymbolById(ref.symbolID)->kind == SymbolKind::Constructor;
      });

    size_t nclasses = std::count_if(begin, end, [&index](const SymbolReference& ref) -> bool {
      return (index.getSymbolById(ref.symbolID)->kind == SymbolKind::Class) 
        || (index.getSymbolById(ref.symbolID)->kind == SymbolKind::Struct);
      });

    size_t nfields = std::count_if(begin, end, [&index](const SymbolReference& ref) -> bool {
      return index.getSymbolById(ref.symbolID)->kind == SymbolKind::Field;
      });

    if (nctor == 1 && nclasses == 1)
    {
      // a constructor and its class name are referenced at the same location.
      // make the class ref implicit.

      std::for_each(begin, end, [&index](SymbolReference& ref) {
        if (index.getSymbolById(ref.symbolID)->kind != SymbolKind::Constructor) {
          ref.flags |= SymbolReference::Implicit;
        }
      });

      return;
    }

    if (nctor == 1 && nfields == 1)
    {
      // TODO: write a test for me!
      // the constructor used to initialize a field is referenced at the same loc
      // as the field. make the ctor ref implicit.

      std::for_each(begin, end, [&index](SymbolReference& ref) {
        if (index.getSymbolById(ref.symbolID)->kind == SymbolKind::Constructor) {
          ref.flags |= SymbolReference::Implicit;
        }
        });

      return;
    }
  }

  // Things get a little tricky here.
  // Here is what we will try to do:
  // - get the token at the specified location
  // - compare the token's text to the name of the referenced symbol
  // - if only one symbol name matches, we mark all other references
  //   as implicit.

  std::string file = indexer.fileIdentificator().getFile(begin->fileID);
  int line = begin->position.line();
  int col = begin->position.column();

  clang::FileID clang_file_id = indexer.getClangFileID(begin->fileID);
  clang::SourceLocation loc = indexer.getAstContext()->getSourceManager().translateLineCol(clang_file_id, line, col);

  if (loc.isValid())
  {
    clang::Token tok;
    if (!clang::Lexer::getRawToken(loc, tok, indexer.getAstContext()->getSourceManager(), indexer.getAstContext()->getLangOpts()))
    {
      std::string spelling = clang::Lexer::getSpelling(tok, indexer.getAstContext()->getSourceManager(), indexer.getAstContext()->getLangOpts());

      size_t ntokmatch = std::count_if(begin, end, [&index, &spelling](const SymbolReference& ref) -> bool {
        return index.getSymbolById(ref.symbolID)->name == spelling;
        });

      if (ntokmatch == 1) {
        std::for_each(begin, end, [&index, &spelling](SymbolReference& ref) {
          if (index.getSymbolById(ref.symbolID)->name != spelling) {
            ref.flags |= SymbolReference::Implicit;
          }
        });

        return;
      }
    }
  }

  //std::cout << n << " (non-implicit) symrefs with same loc @ " << file
  //  << ":" << line << ":" << col << std::endl;
}

static void markImplicitReferences(TranslationUnitIndex& index, Indexer& indexer)
{
  std::vector<SymbolReference>& refs = index.symReferences;

  auto same_loc = [](const SymbolReference& a, const SymbolReference& b) {
    return a.fileID == b.fileID && a.position == b.position;
    };

  auto it = std::adjacent_find(refs.begin(), refs.end(), same_loc);

  while (it != refs.end())
  {
    auto end = std::find_if(it, refs.end(), [&it, &same_loc](const SymbolReference& other) {
      return !same_loc(*it, other);
      });

    markImplicitReferences(index, it, end, indexer);

    it = std::adjacent_find(end, refs.end(), same_loc);
  }
}

void Indexer::finish()
{
  if (m_pp && m_pp->getPreprocessingRecord()) {
    indexPreprocessingRecord(*m_pp);
  }

  // collect ref args
  {
    ClangAstVisitor visitor{ *this };
    visitor.TraverseDecl(getAstContext()->getTranslationUnitDecl());
    sortAndRemoveDuplicates(m_index->fileAnnotations.refargs);
  }

  // if we want to post-process diagnostics, we may use:
  clang::DiagnosticsEngine& de = mAstContext->getDiagnostics();
  clang::DiagnosticConsumer* dc = de.getClient();
  (void)dc;

  sortAndRemoveDuplicates(m_index->symReferences);
  markImplicitReferences(*m_index, *this);

  recordSymbolDeclarations();

  mAstContext = nullptr;
}

void Indexer::HandleDiagnostic(clang::DiagnosticsEngine::Level dlvl, const clang::Diagnostic& dinfo)
{
  assert(initialized());
  if (!initialized())
  {
    return;
  }

  Diagnostic d;

  d.level = static_cast<DiagnosticLevel>(dlvl);

  llvm::SmallString<1000> diag;
  dinfo.FormatDiagnostic(diag);
  d.message = diag.str().str();

  const clang::SourceRange srcrange = dinfo.getLocation();
  clang::PresumedLoc ploc = getSourceManager().getPresumedLoc(srcrange.getBegin());

  if (!ploc.isValid())
  {
    return;
  }

  d.position = FilePosition(ploc.getLine(), ploc.getColumn());

  if (!shouldIndexFile(ploc.getFileID())) {
    return;
  }

  d.fileID = getFileID(ploc.getFileID());

  if (!d.fileID) {
    return;
  }

  if (m_diagnosticConsumer)
  {
    m_diagnosticConsumer->printDiagnostic(d, fileIdentificator().getFile(d.fileID));
  }

  getCurrentIndex()->add(std::move(d));
}

SymbolID Indexer::getMacroSymbolIdFromCache(const clang::MacroInfo* macroInfo) const
{
  return symbolCollector().getMacroSymbolIdFromCache(macroInfo);
}

namespace
{

enum class RelationKind : uint8_t {
  BaseOf = 1,
  OverriddenBy = 2,
};

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
      IndexerSymbol* derived = symbolCollector().process(rel.RelatedSymbol);

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
      IndexerSymbol* base_function = symbolCollector().process(rel.RelatedSymbol);

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

void Indexer::recordSymbolDeclarations()
{
  for (const auto& p : symbolCollector().declarations())
  {
    const IndexerSymbol* symbol = getCurrentIndex()->getSymbolById(p.second);

    if (!symbol) {
      continue;
    }

    const clang::Decl* first = p.first->getCanonicalDecl();
    const clang::Decl* latest = first->getMostRecentDecl();
    const clang::Decl* decl = latest;

    while (decl != nullptr)
    {
      recordSymbolDeclaration(*symbol, *decl);
      decl = decl->getPreviousDecl();
    }
  }

  sortAndRemoveDuplicates(m_index->declarations);
}

void Indexer::recordSymbolDeclaration(const IndexerSymbol& symbol, const clang::Decl& declaration)
{
  bool is_def = false;

  if (const clang::FunctionDecl* fdecl = declaration.getAsFunction())
  {
    is_def = (fdecl->getDefinition() == &declaration);
  }
  else if (const auto* vardecl = llvm::dyn_cast<clang::VarDecl>(&declaration))
  {
    is_def = vardecl->getInit() != nullptr;
  }
  else if (const auto* fielddecl = llvm::dyn_cast<clang::FieldDecl>(&declaration))
  {
    is_def = fielddecl->getInClassInitializer() != nullptr;
  }
  else if (const auto* recdecl = llvm::dyn_cast<clang::RecordDecl>(&declaration))
  {
    is_def = (recdecl->getDefinition() == &declaration);
  }
  else if (const auto* enumdecl = llvm::dyn_cast<clang::EnumDecl>(&declaration))
  {
    is_def = (enumdecl->getDefinition() == &declaration);
  }

  auto should_index_decl = [is_def](const IndexerSymbol& sym) {
    if (sym.testFlag(SymbolFlag::Local)) {
      return false;
    }

    switch (sym.kind) {
    case SymbolKind::Enum:               [[fallthrough]];
    case SymbolKind::EnumClass:          [[fallthrough]];
    case SymbolKind::Struct:             [[fallthrough]];
    case SymbolKind::Class:              [[fallthrough]];
    case SymbolKind::Union:              [[fallthrough]];
    case SymbolKind::Typedef:            [[fallthrough]];
    case SymbolKind::TypeAlias:          [[fallthrough]];
    case SymbolKind::Function:           [[fallthrough]];
    case SymbolKind::Method:             [[fallthrough]];
    case SymbolKind::StaticMethod:       [[fallthrough]];
    case SymbolKind::Constructor:        [[fallthrough]];
    case SymbolKind::Destructor:         [[fallthrough]];
    case SymbolKind::Operator:           [[fallthrough]];
    case SymbolKind::ConversionFunction: [[fallthrough]];
    case SymbolKind::Concept:            return true;
    case SymbolKind::Variable:           [[fallthrough]];
    case SymbolKind::StaticProperty:     [[fallthrough]];
    case SymbolKind::Field:
      return sym.testFlag(VariableInfo::Const | VariableInfo::Constexpr) && is_def;
    default:                             return false;
    }
    };

  if (!should_index_decl(symbol)) {
    return;
  }

  SymbolDeclaration decl;
  decl.symbolID = symbol.id;
  decl.isDefinition = is_def;

  const clang::SourceRange range = declaration.getSourceRange();

  const clang::FileID clang_file_id = getSourceManager().getFileID(range.getEnd());

  if (!shouldIndexFile(clang_file_id)) {
    return;
  }

  std::tie(decl.fileID, decl.startPosition) = convert(range.getBegin());
  std::tie(decl.fileID, decl.endPosition) = convert(range.getEnd());

  getCurrentIndex()->add(decl);
}

} // namespace cppscanner

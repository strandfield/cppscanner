
#include "cppscanner/snapshot/snapshotwriter.h"

#include "cppscanner/index/reference.h"
#include "cppscanner/index/symbolrecords.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace cppscanner::genjs 
{

std::vector<std::string> module_exports;

void write_symbolKinds(std::ofstream& stream)
{
  size_t count = 0;
  std::vector<cppscanner::SymbolKind> names;

  cppscanner::enumerateSymbolKind([&names, &count](cppscanner::SymbolKind k) {
    auto offset = static_cast<size_t>(k);
    if (offset >= names.size()) {
      names.resize(offset + 1, cppscanner::SymbolKind::Unknown);
    }
    names[offset] = k;
    ++count;
    });

  stream << "const symbolKinds = {" << std::endl;
  stream << "  names: [" << std::endl;
  for (size_t i(0); i < names.size(); ++i) {
    stream << "    \"" << cppscanner::getSymbolKindString(names.at(i)) << "\"";
    if (i != names.size() - 1) {
      stream << ",";
    }
    stream << std::endl;
  }
  stream << "  ]," << std::endl;
  stream << "  values: {" << std::endl;
  cppscanner::enumerateSymbolKind([&stream, &names, &count](cppscanner::SymbolKind k) {
    stream << "    \"" << cppscanner::getSymbolKindString(k) << "\": " << static_cast<int>(k);
    if (--count > 0) {
      stream << ",";
    }
    stream << std::endl;
    });
  stream << "  }" << std::endl;
  stream << "};" << std::endl;

  module_exports.push_back("symbolKinds");
}

void write_symbolKindFunctions(std::ofstream& stream)
{
  stream << "function getSymbolKindByName(name) {" << std::endl;
  stream << "  return symbolKinds.values[name];" << std::endl;
  stream << "}" << std::endl;
  module_exports.push_back("getSymbolKindByName");

  stream << "function getSymbolKindValue(nameOrValue) {" << std::endl;
  stream << "  return Number.isInteger(nameOrValue) ? nameOrValue : getSymbolKindByName(nameOrValue);" << std::endl;
  stream << "}" << std::endl;
  module_exports.push_back("getSymbolKindValue");

  const std::string fetchKindAsInteger = "  let k = Number.isInteger(sym.kind) ? sym.kind : symbolKinds.values[sym.kind];";

  auto write_func = [&stream, &fetchKindAsInteger](const std::string& name, cppscanner::SymbolKind k) {
    stream << "function " << name << "(sym) { " << std::endl;
    stream << fetchKindAsInteger << std::endl;
    stream << "  return k == " << static_cast<int>(k) << "; " << std::endl;
    stream << "}" << std::endl;

    module_exports.push_back(name);
    };

  write_func("symbol_isMacro", cppscanner::SymbolKind::Macro);

  auto test = [](cppscanner::SymbolKind k) -> std::string {
    return "k == " + std::to_string(static_cast<int>(k));
    };

  stream << "function symbol_isNamespace(sym) {" << std::endl;
  stream << fetchKindAsInteger << std::endl;
  stream << "  return " << test(cppscanner::SymbolKind::Namespace) << " || " << test(cppscanner::SymbolKind::InlineNamespace) << "; " << std::endl;
  stream << "}" << std::endl;
  module_exports.push_back("symbol_isNamespace");

  stream << "function symbol_isVarLike(sym) {" << std::endl;
  stream << fetchKindAsInteger << std::endl;
  stream << "  return " << test(cppscanner::SymbolKind::Variable) << " || " << test(cppscanner::SymbolKind::Field)
    << " || " << test(cppscanner::SymbolKind::StaticProperty) << "; " << std::endl;
  stream << "}" << std::endl;
  module_exports.push_back("symbol_isVarLike");

  stream << "function symbol_isFunctionLike(sym) {" << std::endl;
  stream << fetchKindAsInteger << std::endl;
  stream << "  return " << test(cppscanner::SymbolKind::Function) << " || " << test(cppscanner::SymbolKind::Method)
    << " || " << test(cppscanner::SymbolKind::StaticMethod) << " || " << test(cppscanner::SymbolKind::Constructor) 
    << " || " << test(cppscanner::SymbolKind::Destructor) 
    << " || " << test(cppscanner::SymbolKind::Operator) 
    << " || " << test(cppscanner::SymbolKind::ConversionFunction) << "; " << std::endl;
  stream << "}" << std::endl;
  module_exports.push_back("symbol_isFunctionLike");
}

void write_symbolFlagFunctions(std::ofstream& stream)
{
  auto write_func = [&stream](const std::string& name, cppscanner::SymbolFlag::Value flag) {
    stream << "function " << name << "(sym) { " << std::endl;
    stream << "  return (sym.flags & " << static_cast<int>(flag) << ") != 0; " << std::endl;
    stream << "}" << std::endl;

    module_exports.push_back(name);
    };

  write_func("symbol_isLocal", cppscanner::SymbolFlag::Local);
  write_func("symbol_isFromProject", cppscanner::SymbolFlag::FromProject);
  write_func("symbol_isProtected", cppscanner::SymbolFlag::Protected);
  write_func("symbol_isPrivate", cppscanner::SymbolFlag::Private);
}

void write_extendedsymbolFlagFunctions(std::ofstream& stream)
{
  auto write_func = [&stream](const std::string& name, auto flag) {
    stream << "function " << name << "(sym) { " << std::endl;
    stream << "  return (sym.flags & " << static_cast<int>(flag) << ") != 0; " << std::endl;
    stream << "}" << std::endl;

    module_exports.push_back(name);
    };

  write_func("macro_isUsedAsHeaderGuard", cppscanner::MacroInfo::MacroUsedAsHeaderGuard);
  write_func("macro_isFunctionLike", cppscanner::MacroInfo::FunctionLike);

  stream << std::endl;

  write_func("variable_isConst", cppscanner::VariableInfo::Const);
  write_func("variable_isConstexpr", cppscanner::VariableInfo::Constexpr);
  write_func("variable_isStatic", cppscanner::VariableInfo::Static);
  write_func("variable_isMutable", cppscanner::VariableInfo::Mutable);  
  write_func("variable_isThreadLocal", cppscanner::VariableInfo::ThreadLocal);
  write_func("variable_isInline", cppscanner::VariableInfo::Inline);

  stream << std::endl;

  write_func("function_isInline", cppscanner::FunctionInfo::Inline);
  write_func("function_isStatic", cppscanner::FunctionInfo::Static);
  write_func("function_isConstexpr", cppscanner::FunctionInfo::Constexpr);
  write_func("function_isConsteval", cppscanner::FunctionInfo::Consteval);
  write_func("function_isNoexcept", cppscanner::FunctionInfo::Noexcept);
  write_func("function_isDefault", cppscanner::FunctionInfo::Default);
  write_func("function_isDelete", cppscanner::FunctionInfo::Delete);
  write_func("function_isConst", cppscanner::FunctionInfo::Const);
  write_func("function_isVirtual", cppscanner::FunctionInfo::Virtual);
  write_func("function_isPure", cppscanner::FunctionInfo::Pure);
  write_func("function_isOverride", cppscanner::FunctionInfo::Override);
  write_func("function_isFinal", cppscanner::FunctionInfo::Final);
  write_func("function_isExplicit", cppscanner::FunctionInfo::Explicit);

  stream << std::endl;

  write_func("class_isFinal", cppscanner::ClassInfo::Final);
}

void write_symbolreferenceFlagFunctions(std::ofstream& stream)
{
  auto write_func = [&stream](const std::string& name, cppscanner::SymbolReference::Flag flag) {
    stream << "function " << name << "(symRef) { " << std::endl;
    stream << "  return (symRef.flags & " << static_cast<int>(flag) << ") != 0; " << std::endl;
    stream << "}" << std::endl;

    module_exports.push_back(name);
    };

  write_func("symbolReference_isDef", cppscanner::SymbolReference::Definition);
  write_func("symbolReference_isDecl", cppscanner::SymbolReference::Declaration);
  write_func("symbolReference_isRead", cppscanner::SymbolReference::Read);
  write_func("symbolReference_isWrite", cppscanner::SymbolReference::Write);
  write_func("symbolReference_isCall", cppscanner::SymbolReference::Call);
  write_func("symbolReference_isDynamic", cppscanner::SymbolReference::Dynamic);
  write_func("symbolReference_isAddressOf", cppscanner::SymbolReference::AddressOf);
  write_func("symbolReference_isImplicit", cppscanner::SymbolReference::Implicit);

  stream << "function symbolReference_isRef(symRef) {" << std::endl;
  stream << "  return !symbolReference_isDef(symRef) && !symbolReference_isDecl(symRef);" << std::endl;
  stream << "}" << std::endl;

  module_exports.push_back("symbolReference_isRef");
}

void write_diagnosticLevels(std::ofstream& stream)
{
  size_t count = 0;
  std::vector<cppscanner::DiagnosticLevel> names;

  cppscanner::enumerateDiagnosticLevel([&names, &count](cppscanner::DiagnosticLevel e) {
    auto offset = static_cast<size_t>(e);
    if (offset >= names.size()) {
      names.resize(offset + 1, cppscanner::DiagnosticLevel::Ignored);
    }
    names[offset] = e;
    ++count;
    });

  stream << "const diagnosticLevels = {" << std::endl;
  stream << "  names: [" << std::endl;
  for (size_t i(0); i < names.size(); ++i) {
    stream << "    \"" << cppscanner::getDiagnosticLevelString(names.at(i)) << "\"";
    if (i != names.size() - 1) {
      stream << ",";
    }
    stream << std::endl;
  }
  stream << "  ]," << std::endl;
  stream << "  values: {" << std::endl;
  cppscanner::enumerateDiagnosticLevel([&stream, &names, &count](cppscanner::DiagnosticLevel e) {
    stream << "    \"" << cppscanner::getDiagnosticLevelString(e) << "\": " << static_cast<int>(e);
    if (--count > 0) {
      stream << ",";
    }
    stream << std::endl;
    });
  stream << "  }" << std::endl;
  stream << "};" << std::endl;

  module_exports.push_back("diagnosticLevels");
}

void write_diagnosticLevelFunctions(std::ofstream& stream)
{
  stream << "function getDiagnosticLevelByName(name) {" << std::endl;
  stream << "  return diagnosticLevels.values[name];" << std::endl;
  stream << "}" << std::endl;
  module_exports.push_back("getDiagnosticLevelByName");

  stream << "function getDiagnosticLevelValue(nameOrValue) {" << std::endl;
  stream << "  return Number.isInteger(nameOrValue) ? nameOrValue : getDiagnosticLevelByName(nameOrValue);" << std::endl;
  stream << "}" << std::endl;
  module_exports.push_back("getDiagnosticLevelValue");

  stream << "function getDiagnosticLevelName(nameOrValue) {" << std::endl;
  stream << "  return Number.isInteger(nameOrValue) ? diagnosticLevels.names[nameOrValue] : nameOrValue;" << std::endl;
  stream << "}" << std::endl;
  module_exports.push_back("getDiagnosticLevelName");
}

void write_moduleExports(std::ofstream& stream)
{
  stream << std::endl;
  stream << "module.exports = {" << std::endl;
  for (size_t i(0); i < module_exports.size(); ++i) {
    stream << "  " << module_exports.at(i);

    if (i != module_exports.size() - 1) {
      stream << ",";
    }

    stream << std::endl;
  }
  stream << "};";
}

} // namespace cppscanner::genjs

int main(int argc, char* argv[])
{
  std::ofstream stream{ "cppscanner.cjs", std::ios::out | std::ios::trunc };

  stream << "// This file was generated by cppscanner." << std::endl;
  stream << "// All modifications will be lost." << std::endl;
  stream << std::endl;

  stream << "const databaseSchemaVersion = " << std::to_string(cppscanner::SnapshotWriter::DatabaseSchemaVersion) << ";" << std::endl;
  stream << std::endl;
  cppscanner::genjs::module_exports.push_back("databaseSchemaVersion");

  cppscanner::genjs::write_symbolKinds(stream);
  stream << std::endl;
  cppscanner::genjs::write_symbolKindFunctions(stream);
  stream << std::endl;
  cppscanner::genjs::write_symbolFlagFunctions(stream);
  stream << std::endl;
  cppscanner::genjs::write_extendedsymbolFlagFunctions(stream);
  stream << std::endl;
  cppscanner::genjs::write_symbolreferenceFlagFunctions(stream);
  stream << std::endl;
  cppscanner::genjs::write_diagnosticLevels(stream);
  stream << std::endl;
  cppscanner::genjs::write_diagnosticLevelFunctions(stream);

  cppscanner::genjs::write_moduleExports(stream);
}

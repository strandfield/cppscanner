
#include "helpers.h"
#include "projects.h"

#include "cppscanner/scannerInvocation/scannerinvocation.h"

#include "cppscanner/database/sql.h"

#include "catch.hpp"

using namespace cppscanner;

static EnumConstantRecord getEnumConstantRecord(const SnapshotReader& s, SymbolID enumId, const std::string& name)
{
  return cppscanner::getRecord<EnumConstantRecord>(s, SymbolRecordFilter().withParent(enumId).withName(name));
}

TEST_CASE("The Scanner runs properly on cxx_language_features", "[scanner][cxx_language_features]")
{
  const std::string snapshot_name = "cxx_language_features.db";

  ScannerInvocation inv{
    {"--compile-commands", CXX_LANGUAGE_FEATURES_BUILD_DIR + std::string("/compile_commands.json"), "--index-external-files", "--index-local-symbols", "--overwrite", "-o", snapshot_name}
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  SnapshotReader s{ snapshot_name };

  std::vector<File> files = s.getFiles();
  REQUIRE(files.size() == 12);
  File maincpp = getFile(files, std::regex("main\\.cpp"));
  File lambdacpp = getFile(files, std::regex("lambda\\.cpp"));

  // lambda
  {
    SymbolRecord fwithlambda = s.getChildSymbolByName("functionWithLambda(int&, int)");
    std::vector<SymbolRecord> alllambdas = s.getChildSymbols(fwithlambda.id, SymbolKind::Lambda);
    REQUIRE(alllambdas.size() == 1);
    const SymbolRecord& lambda = alllambdas.front();
    REQUIRE(!lambda.name.empty());
    REQUIRE(lambda.name.rfind("__lambda_", 0) != std::string::npos);

    SymbolRecord add_to_a = s.getSymbolByName({ "functionWithLambda(int&, int)", "add_to_a" });
    REQUIRE(add_to_a.kind == SymbolKind::Variable);
  }

  // enum
  {
    SymbolRecord enumclass = s.getSymbolByName({ "cxx", "EnumClass" });
    REQUIRE(enumclass.kind == SymbolKind::EnumClass);

    EnumConstantRecord enumclass_z = getEnumConstantRecord(s, enumclass.id, "Z");
    REQUIRE(enumclass_z.expression == "25");
  }
  
  // function template with type parameter
  {
    SymbolRecord incr = getRecord(s, SymbolRecordFilter().withNameLike("max(%)"));
    REQUIRE(incr.kind == SymbolKind::Function);
    std::vector<ParameterRecord> ttps = s.getFunctionParameters(incr.id, SymbolKind::TemplateTypeParameter);
    REQUIRE(ttps.size() == 1);
    ParameterRecord T = ttps.front();
    REQUIRE(T.name == "T");
    REQUIRE(T.parameterIndex == 0);
    REQUIRE(testFlag(T, SymbolFlag::Local));
  }

  // function template with non-type parameter
  {
    FunctionRecord incr = getRecord<FunctionRecord>(s, SymbolRecordFilter().withNameLike("incr(%)"));
    REQUIRE(incr.kind == SymbolKind::Function);
    REQUIRE(incr.returnType == "int");
    std::vector<ParameterRecord> ntps = s.getFunctionParameters(incr.id, SymbolKind::NonTypeTemplateParameter);
    REQUIRE(ntps.size() == 1);
    ParameterRecord N = ntps.front();
    REQUIRE(N.name == "N");
    REQUIRE(N.parameterIndex == 0);
    REQUIRE(N.type == "int");
    REQUIRE(testFlag(N, SymbolFlag::Local));
  }

  // class template with a specialization
  {
    std::vector<SymbolRecord> symbols = s.getSymbolsByName("is_same");
    REQUIRE(symbols.size() == 2);
    SymbolRecord base = symbols.front();
    SymbolRecord spec = symbols.back();

    symbols = s.getChildSymbols(base.id, SymbolKind::TemplateTypeParameter);
    if (symbols.size() != 2)
    {
      std::swap(base, spec);
      symbols = s.getChildSymbols(base.id, SymbolKind::TemplateTypeParameter);
    }

    REQUIRE(symbols.size() == 2);

    {
      std::vector<VariableRecord> properties = s.getStaticProperties(base.id);
      REQUIRE(properties.size() == 1);
      VariableRecord value = properties.front();
      REQUIRE(value.name == "value");
      REQUIRE(testFlag(value, VariableInfo::Constexpr));
      REQUIRE(value.type == "const bool"); // writen as "constexpr bool" in the code; should it be fixed?
      REQUIRE(value.init == "false");
    }

    symbols = s.getChildSymbols(spec.id, SymbolKind::TemplateTypeParameter);
    REQUIRE(symbols.size() == 1);
    REQUIRE(symbols.front().name == "T");
    REQUIRE(testFlag(symbols.front(), SymbolFlag::Local));
    {
      std::vector<VariableRecord> properties = s.getStaticProperties(spec.id);
      REQUIRE(properties.size() == 1);
      VariableRecord value = properties.front();
      REQUIRE(value.name == "value");
      REQUIRE(value.type == "const bool");
      REQUIRE(value.init == "true");
    }
  }

  // class template with a type-alias to the template parameter
  {
    SymbolRecord container_class_template = s.getSymbolByName({"Container"});
    REQUIRE(container_class_template.kind == SymbolKind::Class);
    std::vector<SymbolRecord> tparams =  s.getChildSymbols(container_class_template.id, SymbolKind::TemplateTypeParameter);
    REQUIRE(tparams.size() == 1);

    std::vector<SymbolRecord> typealiases =  s.getChildSymbols(container_class_template.id, SymbolKind::TypeAlias);
    const SymbolRecord& typealias = typealiases.front();
    REQUIRE(typealias.name == "value_type");
    REQUIRE(!testFlag(typealias, SymbolFlag::Local));

    std::vector<SymbolRecord> typedefs =  s.getChildSymbols(container_class_template.id, SymbolKind::Typedef);
    REQUIRE(typedefs.size() == 1);
    const SymbolRecord& thetypedef = typedefs.front();
    REQUIRE(thetypedef.name == "value_type_t");
    REQUIRE(!testFlag(thetypedef, SymbolFlag::Local));
  }

  // test if declaration of variable is definition
  {
    SymbolRecord sym = s.getSymbolByName({"IncompleteType"});
    REQUIRE(testFlag(sym, SymbolFlag::FromProject));
    std::vector<SymbolReference> refs = s.findReferences(sym.id);
    REQUIRE(refs.size() == 1);
    REQUIRE(!bool(refs.front().flags & SymbolReference::Definition));
    REQUIRE(bool(refs.front().flags & SymbolReference::Declaration));

    sym = s.getSymbolByName({"uninitializedGlobalVariable"});
    refs = s.findReferences(sym.id);
    REQUIRE(refs.size() == 1);
    REQUIRE(bool(refs.front().flags & SymbolReference::Definition));

    sym = s.getSymbolByName({"uninitializedStaticGlobalVariable"});
    refs = s.findReferences(sym.id);
    REQUIRE(refs.size() == 1);
    REQUIRE(bool(refs.front().flags & SymbolReference::Definition));

    sym = s.getSymbolByName({"uninitializedExternGlobalVariable"});
    refs = s.findReferences(sym.id);
    REQUIRE(refs.size() == 1);
    REQUIRE(!bool(refs.front().flags & SymbolReference::Definition));
    REQUIRE(bool(refs.front().flags & SymbolReference::Declaration));
  }

  // TODO: tester que l'ouverture d'un namespace ajoute le flag FromProject
}

static MacroRecord getMacroRecordByName(const SnapshotReader& s, const std::string& name)
{
  return cppscanner::getRecord<MacroRecord>(s, SymbolRecordFilter().withName(name));
}

static MacroRecord getMacroRecords(const SnapshotReader& s, const std::string& name)
{
  return cppscanner::getRecord<MacroRecord>(s, SymbolRecordFilter().withName(name));
}

TEST_CASE("Preprocessor macros", "[scanner][cxx_language_features]")
{
  const std::string snapshot_name = "cxx_language_features-macro.db";

  ScannerInvocation inv{
    { "--compile-commands", CXX_LANGUAGE_FEATURES_BUILD_DIR + std::string("/compile_commands.json"),
    "--home", CXX_LANGUAGE_FEATURES_ROOT_DIR,
    "-f:tu", "macro.cpp",
    "--overwrite",
    "-o", snapshot_name }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  TemporarySnapshot s{ snapshot_name };

  std::vector<File> files = s.getFiles();
  REQUIRE(files.size() > 0);
  File macro_cpp = getFile(files, std::regex("macro\\.cpp"));

  MacroRecord my_constant = getMacroRecordByName(s, "MY_CONSTANT");
  MacroRecord greater_than_my_constant = getMacroRecordByName(s, "GREATER_THAN_MY_CONSTANT");
  REQUIRE(my_constant.kind == SymbolKind::Macro);
  REQUIRE(greater_than_my_constant.kind == SymbolKind::Macro);

  std::vector<MacroRecord> my_mins = fetchAll<MacroRecord>(s, SymbolRecordFilter().withName("MY_MIN"));
  REQUIRE(my_mins.size() == 2);
  MacroRecord my_min0 = my_mins[0];
  MacroRecord my_min1 = my_mins[1];
  REQUIRE(my_min0.kind == SymbolKind::Macro);
  REQUIRE(my_min1.kind == SymbolKind::Macro);

  if (my_min0.definition == "min_int(a, b)") {
    std::swap(my_min0, my_min1);
  }

  REQUIRE(my_min0.definition == "( (a) < (b) ? (a) : (b) )");
  REQUIRE(my_min1.definition == "min_int(a, b)");
  REQUIRE(greater_than_my_constant.definition == "(MY_MIN(MY_CONSTANT, (a)) == MY_CONSTANT)");

  MacroRecord my_guard = getMacroRecordByName(s, "MY_GUARD");
  REQUIRE(testFlag(my_guard, MacroInfo::MacroUsedAsHeaderGuard));
}

TEST_CASE("Namespaces", "[scanner][cxx_language_features]")
{
  const std::string snapshot_name = "cxx_language_features-namespace.db";

  ScannerInvocation inv{
    { "--compile-commands", CXX_LANGUAGE_FEATURES_BUILD_DIR + std::string("/compile_commands.json"),
    "--home", CXX_LANGUAGE_FEATURES_ROOT_DIR,
    "-f:tu", "namespace.cpp",
    "--overwrite",
    "-o", snapshot_name }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  TemporarySnapshot s{ snapshot_name };

  SymbolRecord namespaceA = s.getChildSymbolByName("namespaceA");
  SymbolRecord namespaceB = s.getChildSymbolByName("namespaceB");

  NamespaceAliasRecord nsA = s.getNamespaceAliasRecord("nsA");
  NamespaceAliasRecord nsB = s.getNamespaceAliasRecord("nsB");
  REQUIRE(nsA.value == "namespaceA");
  REQUIRE(nsB.value == "nsA::namespaceB");

  SymbolRecord inlNs = s.getChildSymbolByName("inlineNamespace");
  REQUIRE(inlNs.kind == SymbolKind::InlineNamespace);
}

TEST_CASE("goto", "[scanner][cxx_language_features]")
{
  const std::string snapshot_name = "cxx_language_features-goto.db";

  ScannerInvocation inv{
    { "--compile-commands", CXX_LANGUAGE_FEATURES_BUILD_DIR + std::string("/compile_commands.json"),
    "--home", CXX_LANGUAGE_FEATURES_ROOT_DIR,
    "-f:tu", "gotolabel.cpp",
    "--index-local-symbols",
    "--overwrite",
    "-o", snapshot_name }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  SnapshotReader s{ snapshot_name };

  SymbolRecord func = s.getChildSymbolByName("gotolabel(int)");
  REQUIRE(func.kind == SymbolKind::Function);

  SymbolRecord labelA = s.getChildSymbolByName("labelA");
  SymbolRecord labelB = s.getChildSymbolByName("labelB");

  REQUIRE(labelA.kind == labelB.kind);
  REQUIRE(labelA.kind == SymbolKind::GotoLabel);
}

TEST_CASE("converting constructor", "[scanner][cxx_language_features]")
{
  const std::string snapshot_name = "cxx_language_features-converting-ctor.db";

  ScannerInvocation inv{
    { "--compile-commands", CXX_LANGUAGE_FEATURES_BUILD_DIR + std::string("/compile_commands.json"),
    "--home", CXX_LANGUAGE_FEATURES_ROOT_DIR,
    "-f:tu", "converting-constructor.cpp",
    "--overwrite",
    "-o", snapshot_name }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  SnapshotReader s{ snapshot_name };

  SymbolRecord theclass = s.getSymbolByName({ "ClassWithConvertingConstructor" });
  SymbolRecord ctor = s.getChildSymbolByName("ClassWithConvertingConstructor(int)", theclass.id);
  REQUIRE(ctor.kind == SymbolKind::Constructor);

  std::vector<SymbolReference> refs = s.findReferences(ctor.id);
  REQUIRE(refs.size() == 4);
  sort(refs);

  // decl
  REQUIRE(symbolReference_isDef(refs[0]));
  // fConvertingCtor(1);
  REQUIRE(testFlag(refs[1], SymbolReference::Implicit));
  REQUIRE(testFlag(refs[1], SymbolReference::Call));
  // fConvertingCtor({2});
  REQUIRE(testFlag(refs[2], SymbolReference::Implicit));
  REQUIRE(testFlag(refs[2], SymbolReference::Call));
  //fConvertingCtor(ClassWithConvertingConstructor(3));
  REQUIRE(!testFlag(refs[3], SymbolReference::Implicit));
  REQUIRE(testFlag(refs[3], SymbolReference::Call));

  // the class is referenced at the same location as the constructor
  std::vector<SymbolReference> classrefs = s.findReferences(theclass.id);
  sort(classrefs);
  REQUIRE(classrefs.size() > 0);
  REQUIRE(classrefs.back().position == refs[3].position);
  REQUIRE(testFlag(classrefs.back(), SymbolReference::Implicit));
}

TEST_CASE("arguments passed by reference", "[scanner][cxx_language_features]")
{
  const std::string snapshot_name = "cxx_language_features-converting-refargs.db";

  ScannerInvocation inv{
    { "--compile-commands", CXX_LANGUAGE_FEATURES_BUILD_DIR + std::string("/compile_commands.json"),
    "--home", CXX_LANGUAGE_FEATURES_ROOT_DIR,
    "-f:tu", "pass-by-reference.cpp",
    "--index-local-symbols",
    "--overwrite",
    "-o", snapshot_name }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  SnapshotReader s{ snapshot_name };

  std::vector<File> files = s.getFiles();
  File srcfile = getFile(files, std::regex("pass-by-reference\\.cpp"));

  std::vector<ArgumentPassedByReference> refargs = s.getArgumentsPassedByReference(srcfile.id);
  REQUIRE(refargs.size() == 1);
  REQUIRE(refargs.front().position == FilePosition(10, 27));
}


TEST_CASE("declarations", "[scanner][cxx_language_features]")
{
  const std::string snapshot_name = "cxx_language_features-declarations.db";

  ScannerInvocation inv{
    { "--compile-commands", CXX_LANGUAGE_FEATURES_BUILD_DIR + std::string("/compile_commands.json"),
    "--home", CXX_LANGUAGE_FEATURES_ROOT_DIR,
    "-f:tu", "declarations.cpp",
    "--index-local-symbols",
    "--overwrite",
    "-o", snapshot_name }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  SnapshotReader s{ snapshot_name };

  std::vector<File> files = s.getFiles();
  File srcfile = getFile(files, std::regex("declarations\\.cpp"));

  auto sort_by_startline = [](std::vector<SymbolDeclaration>& declarations) {
    std::sort(declarations.begin(), declarations.end(), [](const SymbolDeclaration& a, const SymbolDeclaration& b) {
      return a.startPosition.line() < b.startPosition.line();
      });
  };

  {
    SymbolRecord symbol = s.getSymbolByName({ "integer_t" });
    REQUIRE(symbol.kind == SymbolKind::Typedef);
    std::vector<SymbolDeclaration> declarations = s.getSymbolDeclarations(symbol.id);
    REQUIRE(declarations.size() == 1);
    REQUIRE(declarations.front().fileID == srcfile.id);
    REQUIRE(declarations.front().startPosition.line() == 2);
  }

  {
    SymbolRecord symbol = s.getSymbolByName({ "real" });
    REQUIRE(symbol.kind == SymbolKind::TypeAlias);
    std::vector<SymbolDeclaration> declarations = s.getSymbolDeclarations(symbol.id);
    REQUIRE(declarations.size() == 1);
    REQUIRE(declarations.front().fileID == srcfile.id);
    REQUIRE(declarations.front().startPosition.line() == 4);
  }

  {
    SymbolRecord symbol = s.getSymbolByName({ "Alphabet" });
    REQUIRE(symbol.kind == SymbolKind::Enum);
    std::vector<SymbolDeclaration> declarations = s.getSymbolDeclarations(symbol.id);
    REQUIRE(declarations.size() == 1);
    REQUIRE(declarations.front().fileID == srcfile.id);
    REQUIRE(declarations.front().startPosition.line() == 6);
    REQUIRE(declarations.front().endPosition.line() == 9);
  }

  {
    SymbolRecord symbol = s.getSymbolByName({ "MyClass" });
    REQUIRE(symbol.kind == SymbolKind::Class);
    std::vector<SymbolDeclaration> declarations = s.getSymbolDeclarations(symbol.id);
    REQUIRE(declarations.size() == 2);
    sort_by_startline(declarations);

    REQUIRE(declarations.front().fileID == srcfile.id);
    REQUIRE(declarations.front().startPosition.line() == 11);
    REQUIRE(!declarations.front().isDefinition);

    REQUIRE(declarations.back().fileID == srcfile.id);
    REQUIRE(declarations.back().startPosition.line() == 13);
    REQUIRE(declarations.back().endPosition.line() == 26);
    REQUIRE(declarations.back().isDefinition);
  }

  {
    SymbolRecord symbol = s.getSymbolByName({ "MyClass", "myMethod()" });
    REQUIRE(symbol.kind == SymbolKind::Method);
    std::vector<SymbolDeclaration> declarations = s.getSymbolDeclarations(symbol.id);
    REQUIRE(declarations.size() == 2);
    sort_by_startline(declarations);

    REQUIRE(declarations.front().fileID == srcfile.id);
    REQUIRE(declarations.front().startPosition.line() == 17);
    REQUIRE(!declarations.front().isDefinition);

    REQUIRE(declarations.back().fileID == srcfile.id);
    REQUIRE(declarations.back().startPosition.line() == 28);
    REQUIRE(declarations.back().endPosition.line() == 31);
    REQUIRE(declarations.back().isDefinition);
  }

  {
    SymbolRecord symbol = s.getSymbolByName({ "MyClass", "myInlineMethod()" });
    REQUIRE(symbol.kind == SymbolKind::Method);
    REQUIRE(testFlag(symbol, FunctionInfo::Inline));
    std::vector<SymbolDeclaration> declarations = s.getSymbolDeclarations(symbol.id);
    REQUIRE(declarations.size() == 1);
    REQUIRE(declarations.front().fileID == srcfile.id);
    REQUIRE(declarations.front().startPosition.line() == 19);
    REQUIRE(declarations.front().endPosition.line() == 22);
    REQUIRE(declarations.front().isDefinition);
  }

  {
    SymbolRecord symbol = s.getSymbolByName({ "MyClass", "N" });
    REQUIRE(symbol.kind == SymbolKind::StaticProperty);
    REQUIRE(testFlag(symbol, VariableInfo::Const));
    REQUIRE(testFlag(symbol, VariableInfo::Static));
    std::vector<SymbolDeclaration> declarations = s.getSymbolDeclarations(symbol.id);
    REQUIRE(declarations.size() == 1);
    REQUIRE(declarations.front().fileID == srcfile.id);
    REQUIRE(declarations.front().startPosition.line() == 33);
  }

  {
    SymbolRecord symbol = s.getSymbolByName({ "MyClass", "M" });
    REQUIRE(symbol.kind == SymbolKind::StaticProperty);
    REQUIRE(testFlag(symbol, VariableInfo::Constexpr));
    REQUIRE(testFlag(symbol, VariableInfo::Static));
    std::vector<SymbolDeclaration> declarations = s.getSymbolDeclarations(symbol.id);
    REQUIRE(declarations.size() == 1);
    REQUIRE(declarations.front().fileID == srcfile.id);
    REQUIRE(declarations.front().startPosition.line() == 25);
  }

  {
    SymbolRecord symbol = s.getSymbolByName({ "P" });
    REQUIRE(symbol.kind == SymbolKind::Variable);
    REQUIRE(testFlag(symbol, VariableInfo::Constexpr));
    std::vector<SymbolDeclaration> declarations = s.getSymbolDeclarations(symbol.id);
    REQUIRE(declarations.size() == 1);
    REQUIRE(declarations.front().fileID == srcfile.id);
    REQUIRE(declarations.front().startPosition.line() == 35);
  }

  {
    SymbolRecord symbol = s.getSymbolByName({ "myFunctionDecl(int, int)" });
    REQUIRE(symbol.kind == SymbolKind::Function);
    std::vector<SymbolDeclaration> declarations = s.getSymbolDeclarations(symbol.id);
    REQUIRE(declarations.size() == 3);
    sort_by_startline(declarations);

    REQUIRE(declarations.at(0).fileID == srcfile.id);
    REQUIRE(declarations.at(0).startPosition.line() == 37);
    REQUIRE(!declarations.at(0).isDefinition);

    REQUIRE(declarations.at(1).fileID == srcfile.id);
    REQUIRE(declarations.at(1).startPosition.line() == 39);
    REQUIRE(!declarations.at(1).isDefinition);

    REQUIRE(declarations.back().fileID == srcfile.id);
    REQUIRE(declarations.back().startPosition.line() == 41);
    REQUIRE(declarations.back().endPosition.line() == 44);
    REQUIRE(declarations.back().isDefinition);
  }
}

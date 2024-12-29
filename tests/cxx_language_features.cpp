
#include "helpers.h"
#include "projects.h"

#include "cppscanner/scannerInvocation/scannerinvocation.h"

#include "cppscanner/database/sql.h"

#include "catch.hpp"

using namespace cppscanner;

static const std::string HOME_DIR = TESTFILES_DIRECTORY + std::string("/cxx_language_features");

static EnumConstantRecord getEnumConstantRecord(const SnapshotReader& s, SymbolID enumId, const std::string& name)
{
  return cppscanner::getRecord<EnumConstantRecord>(s, SymbolRecordFilter().withParent(enumId).withName(name));
}

TEST_CASE("main", "[scanner][cxx_language_features]")
{
  const std::string snapshot_name = "cxx_language_features-main.db";
  SnapshotDeleter snapshot_deleter{ snapshot_name };

  ScannerInvocation inv{
    { "run",
    "-i", HOME_DIR + std::string("/main.cpp"),
    "--home", HOME_DIR,
    "--index-local-symbols", "-o", snapshot_name}
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  SnapshotReader s{ snapshot_name };

  std::vector<File> files = s.getFiles();
  REQUIRE(files.size() == 1);
  File maincpp = getFile(files, std::regex("main\\.cpp"));

  // main
  {
    SymbolRecord record = s.getSymbolByName("main()");
    REQUIRE(record.kind == SymbolKind::Function);

    record = s.getSymbolByName("notMain(int, char *[])");
    REQUIRE(record.kind == SymbolKind::Function);
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
}


TEST_CASE("include", "[scanner][cxx_language_features]")
{
  const std::string snapshot_name = "cxx_language_features-include.db";
  SnapshotDeleter snapshot_deleter{ snapshot_name };

  ScannerInvocation inv{
    { "run",
    "-i", HOME_DIR + std::string("/include.cpp"),
    "--home", HOME_DIR,
    "--index-local-symbols",
    "-o", snapshot_name }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  SnapshotReader s{ snapshot_name };

  std::vector<File> files = s.getFiles();
  REQUIRE(files.size() == 2);
  File srcfile = getFile(files, std::regex("include\\.cpp"));
  File hdrfile = getFile(files, std::regex("include\\.h"));

  // pp-includes are indexed correcly
  {
    std::vector<Include> includes = s.getIncludedFiles(srcfile.id);
    REQUIRE(includes.size() == 1);
    REQUIRE(includes.front().includedFileID == hdrfile.id);
    REQUIRE(s.getIncludedFiles(hdrfile.id).empty());
  }

  REQUIRE_NOTHROW(s.getSymbolByName("MyCppStruct"));
  REQUIRE_NOTHROW(s.getSymbolByName("MyIncludedStruct"));
}


TEST_CASE("data members", "[scanner][cxx_language_features]")
{
  const std::string snapshot_name = "cxx_language_features-datamembers.db";
  SnapshotDeleter snapshot_deleter{ snapshot_name };

  ScannerInvocation inv{
    { "run",
    "-i", HOME_DIR + std::string("/datamembers.cpp"),
    "--home", HOME_DIR,
    "-o", snapshot_name }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  SnapshotReader s{ snapshot_name };

  std::vector<File> files = s.getFiles();
  REQUIRE(files.size() ==1);
  File srcfile = getFile(files, std::regex("datamembers\\.cpp"));

  SymbolRecord structFoo = s.getSymbolByName("Foo");
  REQUIRE(structFoo.kind == SymbolKind::Struct);

  // data members are indexed correcly
  {
    SymbolRecord a = s.getChildSymbolByName("a", structFoo.id);
    REQUIRE(a.kind == SymbolKind::Field);
    SymbolRecord b = s.getChildSymbolByName("b", structFoo.id);
    REQUIRE(b.kind == SymbolKind::StaticProperty);
    REQUIRE(testFlag(b, VariableInfo::Static));
    REQUIRE(testFlag(b, VariableInfo::Const));
  }

  // symbol references are indexed correcly
  {
    SymbolRecord a = s.getSymbolByName("a");
    SymbolRecord b = s.getSymbolByName("b");

    std::vector<SymbolReference> refs = s.findReferences(a.id);
    REQUIRE(!refs.empty());
    REQUIRE(refs.size() == 2);

    refs = s.findReferences(b.id);
    REQUIRE(!refs.empty());
    REQUIRE(refs.size() == 2);
    REQUIRE(containsRef(refs, SymbolRefPattern(b).inFile(srcfile).at(5)));
    REQUIRE(containsRef(refs, SymbolRefPattern(b).inFile(srcfile).at(8)));
  }


  // function local symbols are ignored
  {
    REQUIRE_THROWS(s.getSymbolByName("myfoo"));
  }

}

TEST_CASE("inheritance", "[scanner][cxx_language_features]")
{
  const std::string snapshot_name = "cxx_language_features-inheritance.db";
  SnapshotDeleter snapshot_deleter{ snapshot_name };

  ScannerInvocation inv{
    { "run",
    "-i", HOME_DIR + std::string("/inheritance.cpp"),
    "--home", HOME_DIR,
    "--index-local-symbols",
    "-o", snapshot_name }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  SnapshotReader s{ snapshot_name };

  SymbolRecord classBase = s.getSymbolByName("Base");
  SymbolRecord classDerived = s.getSymbolByName("Derived");

  // derived classes are indexed correcly
  {
    std::vector<BaseOf> bases = s.getBasesOf(classDerived.id);
    REQUIRE(bases.size() == 1);
    REQUIRE(bases.front().baseClassID == classBase.id);
    REQUIRE(bases.front().accessSpecifier == AccessSpecifier::Public);
  }

  // method overrides are indexed correcly
  {
    SymbolRecord base_method = s.getChildSymbolByName("vmethod()", classBase.id);
    SymbolRecord derived_method = s.getChildSymbolByName("vmethod()", classDerived.id);
    REQUIRE(testFlag(base_method, FunctionInfo::Virtual));
    REQUIRE(testFlag(base_method, FunctionInfo::Pure));
    REQUIRE(testFlag(derived_method, FunctionInfo::Override));
    REQUIRE(testFlag(derived_method, SymbolFlag::Protected));
    std::vector<Override> overrides = s.getOverridesOf(base_method.id);
    REQUIRE(overrides.size() == 1);
    REQUIRE(overrides.front().overrideMethodID == derived_method.id);
  }

  // global variables are indexed correcly
  {
    SymbolRecord global_var = s.getSymbolByName("gBoolean");
    std::vector<SymbolReference> refs = s.findReferences(global_var.id);
    REQUIRE(refs.size() == 3);
  }
}

TEST_CASE("function", "[scanner][cxx_language_features]")
{
  const std::string snapshot_name = "cxx_language_features-function.db";
  SnapshotDeleter snapshot_deleter{ snapshot_name };

  ScannerInvocation inv{
    { "run",
    "-i", HOME_DIR + std::string("/function.cpp"),
    "--home", HOME_DIR,
    "--index-local-symbols",
    "-o", snapshot_name }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  SnapshotReader s{ snapshot_name };

  // a function inside a namespace is indexed
  {
    SymbolRecord foobar = s.getSymbolByName("foobar");
    REQUIRE(foobar.kind == SymbolKind::Namespace);
    SymbolRecord qux = s.getSymbolByName("qux()");
    REQUIRE(qux.kind == SymbolKind::Function);
    REQUIRE(qux.parentId == foobar.id);
  }
}

TEST_CASE("Enum", "[scanner][cxx_language_features]")
{
  const std::string snapshot_name = "cxx_language_features-enum.db";
  SnapshotDeleter snapshot_deleter{ snapshot_name };

  ScannerInvocation inv{
    { "run",
    "-i", HOME_DIR + std::string("/enum.h"),
    "--home", HOME_DIR,
    "--index-local-symbols",
    "-o", snapshot_name }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  SnapshotReader s{ snapshot_name };

  std::vector<File> files = s.getFiles();
  REQUIRE(files.size() ==1);

  // enum
  {
    SymbolRecord enumclass = s.getSymbolByName({ "cxx", "EnumClass" });
    REQUIRE(enumclass.kind == SymbolKind::EnumClass);

    EnumConstantRecord enumclass_z = getEnumConstantRecord(s, enumclass.id, "Z");
    REQUIRE(enumclass_z.expression == "25");
  }
}

TEST_CASE("Lambda", "[scanner][cxx_language_features]")
{
  const std::string snapshot_name = "cxx_language_features-lambda.db";
  SnapshotDeleter snapshot_deleter{ snapshot_name };

  ScannerInvocation inv{
    { "run",
    "-i", HOME_DIR + std::string("/lambda.cpp"),
    "--home", HOME_DIR,
    "--index-local-symbols",
    "-o", snapshot_name }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  SnapshotReader s{ snapshot_name };

  std::vector<File> files = s.getFiles();
  REQUIRE(files.size() ==1);
  File lambdacpp = getFile(files, std::regex("lambda\\.cpp"));

  // lambda
  {
    SymbolRecord fwithlambda = s.getSymbolByName("functionWithLambda(int&, int)");
    std::vector<SymbolRecord> alllambdas = s.getChildSymbols(fwithlambda.id, SymbolKind::Lambda);
    REQUIRE(alllambdas.size() == 1);
    const SymbolRecord& lambda = alllambdas.front();
    REQUIRE(!lambda.name.empty());
    REQUIRE(lambda.name.rfind("__lambda_", 0) != std::string::npos);

    SymbolRecord add_to_a = s.getSymbolByName({ "functionWithLambda(int&, int)", "add_to_a" });
    REQUIRE(add_to_a.kind == SymbolKind::Variable);
  }
}

TEST_CASE("Template", "[scanner][cxx_language_features]")
{
  const std::string snapshot_name = "cxx_language_features-template.db";
  SnapshotDeleter snapshot_deleter{ snapshot_name };

  ScannerInvocation inv{
    { "run",
    "-i", HOME_DIR + std::string("/template.h"),
    "--home", HOME_DIR,
    "--index-local-symbols",
    "-o", snapshot_name }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  SnapshotReader s{ snapshot_name };

  std::vector<File> files = s.getFiles();
  REQUIRE(files.size() ==1);
  File templateh = getFile(files, std::regex("template\\.h"));

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
  SnapshotDeleter snapshot_deleter{ snapshot_name };

  ScannerInvocation inv{
    { "run",
    "-i", HOME_DIR + std::string("/macro.cpp"),
    "--home", HOME_DIR,
    "-o", snapshot_name }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  SnapshotReader s{ snapshot_name };

  std::vector<File> files = s.getFiles();
  REQUIRE(files.size() == 2);
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
  // TODO: tester que l'ouverture d'un namespace ajoute le flag FromProject


  const std::string snapshot_name = "cxx_language_features-namespace.db";
  SnapshotDeleter snapshot_deleter{ snapshot_name };

  ScannerInvocation inv{
    { "run",
    "-i", HOME_DIR + std::string("/namespace.cpp"),
    "--home", HOME_DIR,
    "-o", snapshot_name }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  SnapshotReader s{ snapshot_name };

  SymbolRecord namespaceA = s.getSymbolByName("namespaceA");
  SymbolRecord namespaceB = s.getSymbolByName("namespaceB");

  NamespaceAliasRecord nsA = s.getNamespaceAliasRecord("nsA");
  NamespaceAliasRecord nsB = s.getNamespaceAliasRecord("nsB");
  REQUIRE(nsA.value == "namespaceA");
  REQUIRE(nsB.value == "nsA::namespaceB");

  SymbolRecord inlNs = s.getSymbolByName("inlineNamespace");
  REQUIRE(inlNs.kind == SymbolKind::InlineNamespace);
}

TEST_CASE("goto", "[scanner][cxx_language_features]")
{
  const std::string snapshot_name = "cxx_language_features-goto.db";
  SnapshotDeleter snapshot_deleter{ snapshot_name };

  ScannerInvocation inv{
    { "run",
    "-i", HOME_DIR + std::string("/gotolabel.cpp"),
    "--home", HOME_DIR,
    "--index-local-symbols",
    "-o", snapshot_name }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  SnapshotReader s{ snapshot_name };

  SymbolRecord func = s.getSymbolByName("gotolabel(int)");
  REQUIRE(func.kind == SymbolKind::Function);

  SymbolRecord labelA = s.getSymbolByName("labelA");
  SymbolRecord labelB = s.getSymbolByName("labelB");

  REQUIRE(labelA.kind == labelB.kind);
  REQUIRE(labelA.kind == SymbolKind::GotoLabel);
}

TEST_CASE("converting constructor", "[scanner][cxx_language_features]")
{
  const std::string snapshot_name = "cxx_language_features-converting-ctor.db";
  SnapshotDeleter snapshot_deleter{ snapshot_name };

  ScannerInvocation inv{
    { "run",
    "-i", HOME_DIR + std::string("/converting-constructor.cpp"),
    "--home", HOME_DIR,
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
  SnapshotDeleter snapshot_deleter{ snapshot_name };

  ScannerInvocation inv{
    { "run",
    "-i", HOME_DIR + std::string("/pass-by-reference.cpp"),
    "--home", HOME_DIR,
    "--index-local-symbols",
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
  SnapshotDeleter snapshot_deleter{ snapshot_name };

  ScannerInvocation inv{
    { "run",
    "-i", HOME_DIR + std::string("/declarations.cpp"),
    "--home", HOME_DIR,
    "--index-local-symbols",
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

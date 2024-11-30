
#include "helpers.h"
#include "projects.h"

#include "cppscanner/scannerInvocation/scannerinvocation.h"

#include "catch.hpp"

using namespace cppscanner;


static std::vector<EnumConstantRecord> getEnumConstantsForEnum(const SnapshotReader& s, SymbolID enumId)
{
  return cppscanner::fetchAll<EnumConstantRecord>(s, SymbolRecordFilter().withParent(enumId));
}


TEST_CASE("The Scanner runs properly on simple_project", "[scanner][simple_project]")
{
  const std::string snapshot_name = "simple_project.db";

  ScannerInvocation inv{
    {"--compile-commands", SIMPLE_PROJECT_BUILD_DIR + std::string("/compile_commands.json"), "--index-external-files", "--overwrite", "-o", snapshot_name}
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  SnapshotReader s{ snapshot_name };

  std::vector<File> files = s.getFiles();
  REQUIRE(files.size() == 2);
  File maincpp = getFile(files, std::regex("main\\.cpp"));
  File mainh = getFile(files, std::regex("main\\.h"));

  // pp-includes are indexed correcly
  {
    std::vector<Include> includes = s.getIncludedFiles(maincpp.id);
    REQUIRE(includes.size() == 1);
    REQUIRE(includes.front().includedFileID == mainh.id);
    REQUIRE(s.getIncludedFiles(mainh.id).empty());
  }

  // enum are indexed correcly
  {
    std::vector<SymbolRecord> symbols = s.getSymbolsByName("ColorChannel");
    REQUIRE(symbols.size() == 1);
    std::vector<EnumConstantRecord> values = getEnumConstantsForEnum(s, symbols.front().id);
    REQUIRE(values.size() == 3);
  }

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

  // symbol references are indexed correcly
  {
    SymbolRecord a = s.getSymbolByName("a");
    SymbolRecord b = s.getSymbolByName("b");

    std::vector<SymbolReference> refs = s.findReferences(a.id);
    REQUIRE(!refs.empty());
    REQUIRE(refs.size() == 2);

    refs = s.findReferences(b.id);
    REQUIRE(!refs.empty());
    REQUIRE(refs.size() == 3);
    REQUIRE(containsRef(refs, SymbolRefPattern(b).inFile(mainh).at(14)));
    REQUIRE(containsRef(refs, SymbolRefPattern(b).inFile(maincpp).at(4)));
    REQUIRE(containsRef(refs, SymbolRefPattern(b).inFile(maincpp).at(10)));
  }

  // global variables are indexed correcly
  {
    REQUIRE_NOTHROW(s.getSymbolByName("gBoolean"));
  }

  // function local symbols are ignored
  {
    REQUIRE_THROWS(s.getSymbolByName("myfoo"));
  }

  // a function inside a namespace is indexed
  {
    SymbolRecord foobar = s.getSymbolByName("foobar");
    REQUIRE(foobar.kind == SymbolKind::Namespace);
    SymbolRecord qux = s.getSymbolByName("qux()");
    REQUIRE(qux.kind == SymbolKind::Function);
    REQUIRE(qux.parentId == foobar.id);
  }
}


#include "helpers.h"
#include "projects.h"

#include "cppscanner/scannerInvocation/scannerinvocation.h"

#include "catch.hpp"

using namespace cppscanner;

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

  auto s = Snapshot::open(snapshot_name);

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
    std::vector<Symbol> symbols = s.findSymbolsByName("ColorChannel");
    REQUIRE(symbols.size() == 1);
    symbols = s.getEnumConstantsForEnum(symbols.front().id);
    REQUIRE(symbols.size() == 3);
  }

  Symbol structFoo = s.getSymbolByName("Foo");
  REQUIRE(structFoo.kind == SymbolKind::Struct);

  // data members are indexed correcly
  {
    Symbol a = s.getSymbolByName("a", structFoo.id);
    REQUIRE(a.kind == SymbolKind::Field);
    Symbol b = s.getSymbolByName("b", structFoo.id);
    REQUIRE(b.kind == SymbolKind::StaticProperty);
    REQUIRE(b.testFlag(Symbol::Static));
    REQUIRE(b.testFlag(Symbol::Const));
  }

  Symbol classBase = s.getSymbolByName("Base");
  Symbol classDerived = s.getSymbolByName("Derived");

  // derived classes are indexed correcly
  {
    std::vector<BaseOf> bases = s.getBasesOf(classDerived.id);
    REQUIRE(bases.size() == 1);
    REQUIRE(bases.front().baseClassID == classBase.id);
    REQUIRE(bases.front().accessSpecifier == AccessSpecifier::Public);
  }

  // method overrides are indexed correcly
  {
    Symbol base_method = s.getSymbolByName("vmethod()", classBase.id);
    Symbol derived_method = s.getSymbolByName("vmethod()", classDerived.id);
    REQUIRE(base_method.testFlag(Symbol::Virtual));
    REQUIRE(base_method.testFlag(Symbol::Pure));
    REQUIRE(derived_method.testFlag(Symbol::Override));
    std::vector<Override> overrides = s.getOverridesOf(base_method.id);
    REQUIRE(overrides.size() == 1);
    REQUIRE(overrides.front().overrideMethodID == derived_method.id);
  }

  // symbol references are indexed correcly
  {
    Symbol a = s.getSymbolByName("a");
    Symbol b = s.getSymbolByName("b");

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
    Symbol foobar = s.getSymbolByName("foobar");
    REQUIRE(foobar.kind == SymbolKind::Namespace);
    Symbol qux = s.getSymbolByName("qux()");
    REQUIRE(qux.kind == SymbolKind::Function);
    REQUIRE(qux.parent_id == foobar.id);
  }
}

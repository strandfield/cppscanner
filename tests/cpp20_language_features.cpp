
#include "helpers.h"
#include "projects.h"

#include "cppscanner/scannerInvocation/scannerinvocation.h"

#include "catch.hpp"

using namespace cppscanner;

TEST_CASE("spaceship operator", "[scanner][cpp20_language_features]")
{
  const std::string snapshot_name = "cpp20_language_features-spaceship.db";

  ScannerInvocation inv{
    { "--compile-commands", CPP20_LANGUAGE_FEATURES_BUILD_DIR + std::string("/compile_commands.json"),
    "--home", CPP20_LANGUAGE_FEATURES_ROOT_DIR,
    "-f:tu", "spaceship.cpp",
    "--overwrite",
    "-o", snapshot_name }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  TemporarySnapshot s{ snapshot_name };

  Symbol intpair = s.getSymbolByName("IntPair");
  Symbol spaceship = s.getSymbolByName("operator<=>(const IntPair&) const", intpair.id);

  REQUIRE(spaceship.testFlag(Symbol::Const));
  REQUIRE(spaceship.testFlag(Symbol::Default));
}

TEST_CASE("designated initializers", "[scanner][cpp20_language_features]")
{
  const std::string snapshot_name = "cpp20_language_features-designated-initializers.db";

  ScannerInvocation inv{
    { "--compile-commands", CPP20_LANGUAGE_FEATURES_BUILD_DIR + std::string("/compile_commands.json"),
    "--home", CPP20_LANGUAGE_FEATURES_ROOT_DIR,
    "-f:tu", "designated-initializers.cpp",
    "--overwrite",
    "-o", snapshot_name }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  TemporarySnapshot s{ snapshot_name };

  Symbol myaggregate = s.getSymbolByName("MyAggregate");
  Symbol flag = s.getSymbolByName("flag", myaggregate.id);
  Symbol n = s.getSymbolByName("n", myaggregate.id);
  Symbol x = s.getSymbolByName("x", myaggregate.id);

  REQUIRE(flag.type == "bool");
  REQUIRE(n.type == "int");
  REQUIRE(x.type == "float");

  Symbol init_aggregate = s.getSymbolByName("init_aggregate()");
  REQUIRE(init_aggregate.kind == SymbolKind::Function);

  std::vector<File> files = s.getFiles();
  REQUIRE(files.size() > 0);
  File srcfile = getFile(files, std::regex("designated-initializers\\.cpp"));

  std::vector<SymbolReference> references = s.findReferences(flag.id);

  auto some_refs_are_in_init_aggregate = [&init_aggregate, &references]() {
    return std::any_of(references.begin(), references.end(), [&init_aggregate](const SymbolReference& ref) {
      return ref.referencedBySymbolID == init_aggregate.id;
      });
    };

  REQUIRE(references.size() == 3);
  REQUIRE(some_refs_are_in_init_aggregate());

  references = s.findReferences(n.id);
  REQUIRE(references.size() == 2);
  REQUIRE(some_refs_are_in_init_aggregate());

  references = s.findReferences(x.id);
  REQUIRE(references.size() == 3);
  REQUIRE(some_refs_are_in_init_aggregate());
}

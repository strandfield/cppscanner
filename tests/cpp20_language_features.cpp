
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

  SymbolRecord intpair = s.getChildSymbolByName("IntPair");
  SymbolRecord spaceship = s.getChildSymbolByName("operator<=>(const IntPair&) const", intpair.id);

  REQUIRE(testFlag(spaceship, FunctionInfo::Const));
  REQUIRE(testFlag(spaceship, FunctionInfo::Default));
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

  SymbolRecord myaggregate = s.getChildSymbolByName("MyAggregate");
  VariableRecord flag = s.getField(myaggregate.id, "flag");
  VariableRecord n = s.getField(myaggregate.id, "n");
  VariableRecord x = s.getField(myaggregate.id, "x");

  REQUIRE(flag.type == "bool");
  REQUIRE(n.type == "int");
  REQUIRE(x.type == "float");

  SymbolRecord init_aggregate = s.getChildSymbolByName("init_aggregate()");
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

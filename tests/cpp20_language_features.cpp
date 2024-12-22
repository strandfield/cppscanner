
#include "helpers.h"
#include "projects.h"

#include "cppscanner/scannerInvocation/scannerinvocation.h"

#include "catch.hpp"

using namespace cppscanner;

static const std::string HOME_DIR = TESTFILES_DIRECTORY + std::string("/cpp20_language_features");

TEST_CASE("spaceship operator", "[scanner][cpp20_language_features]")
{
  const std::string snapshot_name = "cpp20_language_features-spaceship.db";

  ScannerInvocation inv{
    { "-i", HOME_DIR + std::string("/spaceship.cpp"),
    "--home", HOME_DIR,
    "--overwrite",
    "-o", snapshot_name,
    "--", "-std=c++20"}
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  SnapshotReader s{ snapshot_name };

  SymbolRecord intpair = s.getSymbolByName("IntPair");
  SymbolRecord spaceship = s.getChildSymbolByName("operator<=>(const IntPair&) const", intpair.id);

  REQUIRE(testFlag(spaceship, FunctionInfo::Const));
  REQUIRE(testFlag(spaceship, FunctionInfo::Default));
}

TEST_CASE("designated initializers", "[scanner][cpp20_language_features]")
{
  const std::string snapshot_name = "cpp20_language_features-designated-initializers.db";

  ScannerInvocation inv{
    { "-i", HOME_DIR + std::string("/designated-initializers.cpp"),
    "--home", HOME_DIR,
    "--overwrite",
    "-o", snapshot_name,
    "--", "-std=c++20" }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  SnapshotReader s{ snapshot_name };

  SymbolRecord myaggregate = s.getSymbolByName("MyAggregate");
  VariableRecord flag = s.getField(myaggregate.id, "flag");
  VariableRecord n = s.getField(myaggregate.id, "n");
  VariableRecord x = s.getField(myaggregate.id, "x");

  REQUIRE(flag.type == "bool");
  REQUIRE(n.type == "int");
  REQUIRE(x.type == "float");

  SymbolRecord init_aggregate = s.getSymbolByName("init_aggregate()");
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

// https://en.cppreference.com/w/cpp/language/constraints
TEST_CASE("Constraints and concepts", "[scanner][cpp20_language_features]")
{
  const std::string snapshot_name = "cpp20_language_features-constraints.db";

  ScannerInvocation inv{
    { "-i", HOME_DIR + std::string("/concepts.cpp"),
    "--home", HOME_DIR,
    "--overwrite",
    "-o", snapshot_name,
    "--", "-std=c++20"}
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  SnapshotReader s{ snapshot_name };

  {
    SymbolRecord floating_point = s.getSymbolByName("floating_point");
    REQUIRE(floating_point.kind == SymbolKind::Concept);
    REQUIRE(testFlag(floating_point, SymbolFlag::FromProject) == false);
  }

  {
    SymbolRecord number = s.getSymbolByName("Number");
    REQUIRE(number.kind == SymbolKind::Concept);
    REQUIRE(testFlag(number, SymbolFlag::FromProject));

    std::vector<SymbolReference> refs = s.findReferences(number.id);
    REQUIRE(refs.size() == 3);

    size_t l21 = std::count_if(refs.begin(), refs.end(), [](const SymbolReference& elem) {
      return elem.position.line() == 21;
      });
    REQUIRE(l21 == 2);

    std::vector<SymbolDeclaration> decls = s.getSymbolDeclarations(number.id);
    REQUIRE(decls.size() == 1);
    SymbolDeclaration decl = decls.at(0);
    REQUIRE((decl.startPosition.line() == 11 && decl.endPosition.line() == 12));
  }

  {
    SymbolRecord iterable = s.getSymbolByName("Iterable");
    REQUIRE(iterable.kind == SymbolKind::Concept);
    REQUIRE(testFlag(iterable, SymbolFlag::FromProject));


    std::vector<SymbolReference> refs = s.findReferences(iterable.id);
    REQUIRE(refs.size() == 2);

    std::vector<SymbolDeclaration> decls = s.getSymbolDeclarations(iterable.id);
    REQUIRE(decls.size() == 1);
  }
}
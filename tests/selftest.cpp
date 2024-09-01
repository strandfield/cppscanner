
#include "helpers.h"

#include "cppscanner/scannerInvocation/scannerinvocation.h"
#include "cppscanner/index/symbol.h"
#include "cppscanner/database/sql.h"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

using namespace cppscanner;

#include <iostream>
#include <chrono>

TEST_CASE("Self parsing test", "[scanner][self]")
{
  std::cout << SELFTEST_BUILD_DIR << std::endl;

  const std::string snapshot_name = "self.db";

  ScannerInvocation inv{
    { "--compile-commands", SELFTEST_BUILD_DIR + std::string("/compile_commands.json"),
    "--home", PROJECT_SOURCE_DIR,
    "--project-name", "cppscanner",
    "--overwrite",
    "-o", snapshot_name }
  };

  auto start = std::chrono::high_resolution_clock::now();

  // the scanner invocation succeeds
  { 
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  std::cout << "Self parse succeeded in " << duration.count() << "ms." << std::endl;

  TemporarySnapshot s{ snapshot_name };

  // verify the presence of some classes
  {
    Symbol ns = s.getSymbolByName("cppscanner");
    REQUIRE(ns.kind == SymbolKind::Namespace);
    Symbol indexer = s.getSymbolByName("Indexer", ns.id);
    REQUIRE(indexer.kind == SymbolKind::Class);
    Symbol scanner = s.getSymbolByName("Scanner", ns.id);
    REQUIRE(scanner.kind == SymbolKind::Class);
  }

  // Indexer derives from clang
  {
    Symbol indexer = s.getSymbol({ "cppscanner", "Indexer" });
    REQUIRE(indexer.kind == SymbolKind::Class);
    
    std::vector<BaseOf> bases = s.getBasesOf(indexer.id);
    REQUIRE(bases.size() == 1);

    Symbol idxdtcon = s.getSymbol({ "clang", "index", "IndexDataConsumer" });
    REQUIRE(indexer.kind == SymbolKind::Class);
    REQUIRE(bases.front().baseClassID == idxdtcon.id);

    Symbol handle_decl_occurrence_derived = s.getSymbolByName(sql::Like("handleDeclOccurrence(%)"), indexer.id);
    REQUIRE(handle_decl_occurrence_derived.kind == SymbolKind::InstanceMethod);
    REQUIRE(handle_decl_occurrence_derived.testFlag(Symbol::Final));

    Symbol handle_decl_occurrence_base = s.getSymbolByName(sql::Like("handleDeclOccurrence(%)"), idxdtcon.id);
    std::vector<Override> overrides = s.getOverridesOf(handle_decl_occurrence_base.id);
    REQUIRE(!overrides.empty());
    REQUIRE(overrides.size() == 1);
    REQUIRE(overrides.front().overrideMethodID == handle_decl_occurrence_derived.id);
  }
}

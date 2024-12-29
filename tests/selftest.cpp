
#include "helpers.h"

#include "cppscanner/scannerInvocation/scannerinvocation.h"
#include "cppscanner/index/symbol.h"
#include "cppscanner/database/sql.h"
#include "cppscanner/base/version.h"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

using namespace cppscanner;

#include <iostream>
#include <chrono>

TEST_CASE("Self parsing test", "[scanner][self]")
{
  const std::string snapshot_name = "cppscanner.db";

  {
    auto start = std::chrono::high_resolution_clock::now();

    ScannerInvocation inv{
      { "run",
      "--compile-commands", SELFTEST_BUILD_DIR + std::string("/compile_commands.json"),
      "--home", PROJECT_SOURCE_DIR,
      "--remap-file-ids",
      "--index-local-symbols",
      "--threads", "2",
      "--project-name", "cppscanner",
      "--project-version", cppscanner::versioncstr(),
      "--overwrite",
      "-o", snapshot_name }
    };

    REQUIRE_NOTHROW(inv.run());
    CHECK(inv.errors().empty());
  
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Self parse completed in " << duration.count() << "ms." << std::endl;
  }

  SnapshotReader s{ snapshot_name };

  // verify the presence of some classes
  {
    SymbolRecord ns = s.getSymbolByName("cppscanner");
    REQUIRE(ns.kind == SymbolKind::Namespace);
    SymbolRecord indexer = s.getChildSymbolByName("Indexer", ns.id);
    REQUIRE(indexer.kind == SymbolKind::Class);
    SymbolRecord scanner = s.getChildSymbolByName("Scanner", ns.id);
    REQUIRE(scanner.kind == SymbolKind::Class);
  }

  // Indexer derives from clang
  {
    {
      SymbolRecord indexer = s.getSymbolByName({ "cppscanner", "Indexer" });
      REQUIRE(indexer.kind == SymbolKind::Class);
    }

    SymbolRecord idc = s.getSymbolByName({ "cppscanner", "ForwardingIndexDataConsumer" });
    REQUIRE(idc.kind == SymbolKind::Class);
    std::vector<BaseOf> bases = s.getBasesOf(idc.id);
    REQUIRE(bases.size() == 1);

    SymbolRecord idxdtcon = s.getSymbolByName({ "clang", "index", "IndexDataConsumer" });
    REQUIRE(idxdtcon.kind == SymbolKind::Class);
    REQUIRE(bases.front().baseClassID == idxdtcon.id);

    SymbolRecord handle_decl_occurrence_derived = getRecord(s, SymbolRecordFilter().withNameLike("handleDeclOccurrence(%)").withParent(idc.id));
    REQUIRE(handle_decl_occurrence_derived.kind == SymbolKind::Method);
    REQUIRE(testFlag(handle_decl_occurrence_derived, FunctionInfo::Override));

    SymbolRecord handle_decl_occurrence_base = getRecord(s, SymbolRecordFilter().withNameLike("handleDeclOccurrence(%)").withParent(idxdtcon.id));
    std::vector<Override> overrides = s.getOverridesOf(handle_decl_occurrence_base.id);
    REQUIRE(!overrides.empty());
    REQUIRE(overrides.size() == 1);
    REQUIRE(overrides.front().overrideMethodID == handle_decl_occurrence_derived.id);
  }
}


#include "helpers.h"
#include "cppscanner-cmake.h"

#include "cppscanner/cmakeIntegration/cmakeproject.h"
#include "cppscanner/scannerInvocation/scannerinvocation.h"
#include "cppscanner/scannerInvocation/cmakeinvocation.h"
#include "cppscanner/indexer/version.h"
#include "cppscanner/index/symbol.h"
#include "cppscanner/database/sql.h"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

using namespace cppscanner;

#include <iostream>
#include <chrono>

TEST_CASE("Self parsing test", "[scanner][self]")
{
  const std::string snapshot_name = "cppscanner.db";
  const std::string build_dirname = "cppscanner_self_parse";

  {
    auto start = std::chrono::high_resolution_clock::now();

    const std::string build_dir = (std::filesystem::path(CMAKE_BINARY_DIR) / build_dirname).generic_u8string();

    CMakeCommandInvocation cmake{
      { 
        "-B", build_dir,
        "-S", CMAKE_SOURCE_DIR,
        "-G", CMAKE_GENERATOR,
        "-DCMAKE_PREFIX_PATH=" + std::string(CMAKE_PREFIX_PATH),
        "-DLLVM_DIR=" + std::string(LLVM_DIR), 
        "-DBUILD_TESTS=OFF",
        "-DSELF_PARSE=ON",
      }
    };

    REQUIRE(cmake.exec());

    ScannerInvocation inv{
      { "--build", build_dir,
      "--target", CMakeTarget::all(),
      "--home", CMAKE_SOURCE_DIR,
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
    SymbolRecord indexer = s.getSymbolByName({ "cppscanner", "Indexer" });
    REQUIRE(indexer.kind == SymbolKind::Class);
    
    std::vector<BaseOf> bases = s.getBasesOf(indexer.id);
    REQUIRE(bases.size() == 1);

    SymbolRecord idxdtcon = s.getSymbolByName({ "clang", "index", "IndexDataConsumer" });
    REQUIRE(indexer.kind == SymbolKind::Class);
    REQUIRE(bases.front().baseClassID == idxdtcon.id);

    SymbolRecord handle_decl_occurrence_derived = getRecord(s, SymbolRecordFilter().withNameLike("handleDeclOccurrence(%)").withParent(indexer.id));
    REQUIRE(handle_decl_occurrence_derived.kind == SymbolKind::Method);
    REQUIRE(testFlag(handle_decl_occurrence_derived, FunctionInfo::Final));

    SymbolRecord handle_decl_occurrence_base = getRecord(s, SymbolRecordFilter().withNameLike("handleDeclOccurrence(%)").withParent(idxdtcon.id));
    std::vector<Override> overrides = s.getOverridesOf(handle_decl_occurrence_base.id);
    REQUIRE(!overrides.empty());
    REQUIRE(overrides.size() == 1);
    REQUIRE(overrides.front().overrideMethodID == handle_decl_occurrence_derived.id);
  }
}

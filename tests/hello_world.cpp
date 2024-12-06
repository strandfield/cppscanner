
#include "helpers.h"
#include "projects.h"

#include "cppscanner/cmakeIntegration/cmakeproject.h"
#include "cppscanner/scannerInvocation/cmakeinvocation.h"
#include "cppscanner/scannerInvocation/scannerinvocation.h"

#include "cppscanner/database/sql.h"

#include "catch.hpp"

using namespace cppscanner;

TEST_CASE("hello_world test project", "[scanner][hello_world]")
{
  std::vector<std::string> snapshots;

  SECTION("compile_commands.json") {
    const std::string snapshot_name = "hello_world-cc.db";

    ScannerInvocation inv{
      { "--compile-commands", HELLO_WORLD_BUILD_DIR + std::string("/compile_commands.json"),
      "--home", HELLO_WORLD_ROOT_DIR,
      "--overwrite",
      "-o", snapshot_name }
    };

    REQUIRE_NOTHROW(inv.run());
    CHECK(inv.errors().empty());

    if (inv.errors().empty()) {
      snapshots.push_back(snapshot_name);
    }
  }

  SECTION("cmake") {
    CMakeCommandInvocation cmake{
      { "-B", HELLO_WORLD_BUILD_DIR,
      "-S", HELLO_WORLD_ROOT_DIR }
    };

    cmake.exec();

    CMakeIndex cmkindex;
    REQUIRE(cmkindex.read(HELLO_WORLD_BUILD_DIR));

    const std::string snapshot_name = "hello_world-cmake.db";

#ifdef _WIN32
    const std::string all = "ALL_BUILD";
#else
    const std::string all = "all";
#endif // _WIN32

    ScannerInvocation inv{
      { "--build", HELLO_WORLD_BUILD_DIR,
      "--target", all,
      "--home", HELLO_WORLD_ROOT_DIR,
      "--overwrite",
      "-o", snapshot_name }
    };

    REQUIRE_NOTHROW(inv.run());
    CHECK(inv.errors().empty());

    if (inv.errors().empty()) {
      snapshots.push_back(snapshot_name);
    }
  }

  for (const std::string& snapshot_name : snapshots)
  {
    SnapshotReader s{ snapshot_name };

    std::vector<File> files = s.getFiles();
    REQUIRE(files.size() >= 2);
    File srcfile = getFile(files, std::regex("main\\.cpp"));
    File hdrfile = getFile(files, std::regex("hello\\.h"));
    File iostream = getFile(files, std::regex("iostream"));

    // pp-includes are indexed correcly
    {
      std::vector<Include> includes = s.getIncludedFiles(srcfile.id);
      REQUIRE(includes.size() == 1);
      REQUIRE(includes.front().includedFileID == hdrfile.id);

      includes = s.getIncludedFiles(hdrfile.id);
      REQUIRE(includes.size() == 1);
      REQUIRE(includes.front().includedFileID == iostream.id);
    }

    SymbolRecord stdns = s.getSymbolByName("std");
    REQUIRE(stdns.kind == SymbolKind::Namespace);
    SymbolRecord stdcout = s.getSymbolByName("cout");
    SymbolRecord stdendl = getRecord(s, SymbolRecordFilter().withNameLike("endl(%)").withParent(stdns.id));

    // references to std symbols are correct
    {
      std::vector<SymbolReference> refs = s.findReferences(stdcout.id);
      REQUIRE(!refs.empty());
      filterRefs(refs, SymbolRefPattern(stdcout).inFile(hdrfile));
      REQUIRE(refs.size() == 1);

      refs = s.findReferences(stdendl.id);
      REQUIRE(!refs.empty());
      filterRefs(refs, SymbolRefPattern(stdendl).inFile(hdrfile));
      REQUIRE(refs.size() == 1);
    }
  }
}


#include "helpers.h"
#include "projects.h"

#include "cppscanner/scannerInvocation/scannerinvocation.h"

#include "cppscanner/database/sql.h"

#include "catch.hpp"

using namespace cppscanner;

TEST_CASE("hello_world test project", "[scanner][hello_world]")
{
  const std::string snapshot_name = "hello_world.db";

  ScannerInvocation inv{
    { "run",
    "--compile-commands", HELLO_WORLD_BUILD_DIR + std::string("/compile_commands.json"),
    "--home", HELLO_WORLD_ROOT_DIR,
    "--remap-file-ids",
    "--overwrite",
    "-o", snapshot_name }
  };

  REQUIRE_NOTHROW(inv.run());
  CHECK(inv.errors().empty());

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

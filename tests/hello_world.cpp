
#include "helpers.h"
#include "projects.h"

#include "cppscanner/scannerInvocation/scannerinvocation.h"

#include "cppscanner/database/sql.h"

#include "catch.hpp"

using namespace cppscanner;

TEST_CASE("The Scanner runs properly on hello_world", "[scanner][hello_world]")
{
  const std::string snapshot_name = "hello_world.db";

  ScannerInvocation inv{
    { "--compile-commands", HELLO_WORLD_BUILD_DIR + std::string("/compile_commands.json"),
      "--home", HELLO_WORLD_ROOT_DIR,
      "--overwrite",
      "-o", snapshot_name }
  };

  // the scanner invocation succeeds"
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  auto s = Snapshot::open(snapshot_name);

  std::vector<File> files = s.getFiles();
  REQUIRE(files.size() >= 2);
  File hellocpp = getFile(files, std::regex("hello\\.cpp"));
  File iostream = getFile(files, std::regex("iostream"));

  // pp-includes are indexed correcly
  {
    std::vector<Include> includes = s.getIncludedFiles(hellocpp.id);
    REQUIRE(includes.size() == 1);
    REQUIRE(includes.front().includedFileID == iostream.id);
  }

  SymbolRecord stdns = s.getSymbolByName("std");
  REQUIRE(stdns.kind == SymbolKind::Namespace);
  SymbolRecord stdcout = s.getSymbolByName("cout");
  SymbolRecord stdendl = s.getSymbolByName(sql::Like("endl(%)"), stdns.id);

  // references to std symbols are correct
  {
    std::vector<SymbolReference> refs = s.findReferences(stdcout.id);
    REQUIRE(!refs.empty());
    filterRefs(refs, SymbolRefPattern(stdcout).inFile(hellocpp));
    REQUIRE(refs.size() == 1);

    refs = s.findReferences(stdendl.id);
    REQUIRE(!refs.empty());
    filterRefs(refs, SymbolRefPattern(stdendl).inFile(hellocpp));
    REQUIRE(refs.size() == 1);
  }
}

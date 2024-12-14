
#include "helpers.h"
#include "projects.h"

#include "cppscanner/cmakeIntegration/cmakeproject.h"
#include "cppscanner/scannerInvocation/cmakeinvocation.h"
#include "cppscanner/scannerInvocation/scannerinvocation.h"

#include "cppscanner/database/sql.h"

#include "catch.hpp"

using namespace cppscanner;

TEST_CASE("includedir", "[scanner][cmake]")
{
  CMakeCommandInvocation cmake{
    { 
      "-B", INCLUDEDIR_BUILD_DIR,
      "-S", INCLUDEDIR_ROOT_DIR 
    }
  };

  cmake.exec();

  CMakeIndex cmkindex;
  REQUIRE(cmkindex.read(INCLUDEDIR_BUILD_DIR));

  const std::string snapshot_name = "includedir.db";

  ScannerInvocation inv{
    { "--build", INCLUDEDIR_BUILD_DIR,
    "--target", CMakeTarget::all(),
    "--home", INCLUDEDIR_ROOT_DIR,
    "--overwrite",
    "-o", snapshot_name }
  };

  REQUIRE_NOTHROW(inv.run());
  CHECK(inv.errors().empty());

  SnapshotReader s{ snapshot_name };

  std::vector<File> files = s.getFiles();
  REQUIRE(files.size() == 2);
  File srcfile = getFile(files, std::regex("main\\.cpp"));
  File hdrfile = getFile(files, std::regex("defines\\.h"));

  // pp-includes are indexed correcly
  {
    std::vector<Include> includes = s.getIncludedFiles(srcfile.id);
    REQUIRE(includes.size() == 1);
    REQUIRE(includes.front().includedFileID == hdrfile.id);
  }

  auto variable = getRecord<VariableRecord>(s, SymbolRecordFilter().withName("RET_CODE"));
  REQUIRE(variable.type == "const int");
  REQUIRE(testFlag(variable, VariableInfo::Constexpr));
  REQUIRE(variable.init == "0");
}


TEST_CASE("cmake_compile_definitions", "[scanner][cmake]")
{
  CMakeCommandInvocation cmake{
    { 
      "-B", CMAKE_COMPILE_DEFINITIONS_BUILD_DIR,
      "-S", CMAKE_COMPILE_DEFINITIONS_ROOT_DIR 
    }
  };

  cmake.exec();

  const std::string snapshot_name = "cmake_compile_definitions.db";

  ScannerInvocation inv{
    { "--build", CMAKE_COMPILE_DEFINITIONS_BUILD_DIR,
    "--target", CMakeTarget::all(),
    "--home", CMAKE_COMPILE_DEFINITIONS_ROOT_DIR,
    "--overwrite",
    "-o", snapshot_name }
  };

  REQUIRE_NOTHROW(inv.run());
  CHECK(inv.errors().empty());

  SnapshotReader s{ snapshot_name };

  std::vector<File> files = s.getFiles();
  REQUIRE(files.size() ==1);
  File srcfile = getFile(files, std::regex("main\\.cpp"));

  std::vector<Diagnostic> diagnostics = s.getDiagnostics();
  REQUIRE(diagnostics.empty());

  auto macro = getRecord<MacroRecord>(s, SymbolRecordFilter().withName("I_AM_THE_ONE"));
  REQUIRE(!testFlag(macro, MacroInfo::MacroUsedAsHeaderGuard));
  REQUIRE(!testFlag(macro, MacroInfo::FunctionLike));

  //REQUIRE(macro.definition == "1"); // TODO: do that?
}

TEST_CASE("cmake_precompile_headers", "[scanner][cmake]")
{
  CMakeCommandInvocation cmake{
    { 
      "-B", CMAKE_PRECOMPILE_HEADERS_BUILD_DIR,
      "-S", CMAKE_PRECOMPILE_HEADERS_ROOT_DIR 
    }
  };

  cmake.exec();

  const std::string snapshot_name = "cmake_precompile_headers.db";

  ScannerInvocation inv{
    { "--build", CMAKE_PRECOMPILE_HEADERS_BUILD_DIR,
    "--target", CMakeTarget::all(),
    "--home", CMAKE_PRECOMPILE_HEADERS_ROOT_DIR,
    "--overwrite",
    "-o", snapshot_name }
  };

  REQUIRE_NOTHROW(inv.run());
  CHECK(inv.errors().empty());

  SnapshotReader s{ snapshot_name };

  std::vector<File> files = s.getFiles();
  File srcfile = getFile(files, std::regex("main\\.cpp"));
  File cmake_pch = getFile(files, std::regex("cmake_pch\\.cxx"));

  std::vector<Diagnostic> diagnostics = s.getDiagnostics();
  REQUIRE(diagnostics.empty());
}
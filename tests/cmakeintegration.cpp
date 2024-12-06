
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

#ifdef _WIN32
  const std::string all = "ALL_BUILD";
#else
  const std::string all = "all";
#endif // _WIN32

  ScannerInvocation inv{
    { "--build", INCLUDEDIR_BUILD_DIR,
    "--target", all,
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

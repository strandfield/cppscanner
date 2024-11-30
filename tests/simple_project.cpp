
#include "helpers.h"
#include "projects.h"

#include "cppscanner/scannerInvocation/scannerinvocation.h"

#include "catch.hpp"

using namespace cppscanner;


static std::vector<EnumConstantRecord> getEnumConstantsForEnum(const SnapshotReader& s, SymbolID enumId)
{
  return cppscanner::fetchAll<EnumConstantRecord>(s, SymbolRecordFilter().withParent(enumId));
}


TEST_CASE("The Scanner runs properly on simple_project", "[scanner][simple_project]")
{
  const std::string snapshot_name = "simple_project.db";

  ScannerInvocation inv{
    {"--compile-commands", SIMPLE_PROJECT_BUILD_DIR + std::string("/compile_commands.json"), "--index-external-files", "--overwrite", "-o", snapshot_name}
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  SnapshotReader s{ snapshot_name };

  std::vector<File> files = s.getFiles();
  REQUIRE(files.size() == 2);
  File maincpp = getFile(files, std::regex("main\\.cpp"));
  File mainh = getFile(files, std::regex("main\\.h"));

}

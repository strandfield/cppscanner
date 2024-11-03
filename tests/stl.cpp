
#include "helpers.h"
#include "projects.h"

#include "cppscanner/scannerInvocation/scannerinvocation.h"

#include "catch.hpp"

using namespace cppscanner;

TEST_CASE("string.cpp", "[scanner][stl]")
{
  const std::string snapshot_name = "stl-string.db";

  ScannerInvocation inv{
    { "--compile-commands", STL_BUILD_DIR + std::string("/compile_commands.json"),
      "--home", STL_ROOT_DIR,
      "-f:tu", "string.cpp",
      "--overwrite",
      "-o", snapshot_name }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  TemporarySnapshot s{ snapshot_name };

  std::vector<File> files = s.getFiles();
  REQUIRE(files.size() > 0);
  File stringcpp = getFile(files, std::regex("string\\.cpp"));
  REQUIRE_THROWS(getFile(files, std::regex("vector\\.cpp")));

  SymbolRecord init = s.getChildSymbolByName("init()");
  REQUIRE(init.kind == SymbolKind::Function);

  // TODO: tester que std::string n'a pas le flag FromProject
}

TEST_CASE("vector.cpp", "[scanner][stl]")
{
  const std::string snapshot_name = "stl-vector.db";

  ScannerInvocation inv{
    { "--compile-commands", STL_BUILD_DIR + std::string("/compile_commands.json"),
    "--home", STL_ROOT_DIR,
    "-f:tu", "vector.cpp",
    "--overwrite",
    "-o", snapshot_name }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  TemporarySnapshot s{ snapshot_name };

  std::vector<File> files = s.getFiles();
  REQUIRE(files.size() > 0);
  File vectorcpp = getFile(files, std::regex("vector\\.cpp"));
  REQUIRE_THROWS(getFile(files, std::regex("string\\.cpp")));

  SymbolRecord sortThisVector = s.getChildSymbolByName("sortThisVector(std::vector<int>&)");
  REQUIRE(sortThisVector.kind == SymbolKind::Function);
}

TEST_CASE("tuple.cpp", "[scanner][stl]")
{
  const std::string snapshot_name = "stl-tuple.db";

  ScannerInvocation inv{
    { "--compile-commands", STL_BUILD_DIR + std::string("/compile_commands.json"),
    "--home", STL_ROOT_DIR,
    "-f:tu", "tuple.cpp",
    "--overwrite",
    "-o", snapshot_name }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  SnapshotReader s{ snapshot_name };

  std::vector<File> files = s.getFiles();
  REQUIRE(files.size() > 0);
  File tuplecpp = getFile(files, std::regex("tuple\\.cpp"));
}

TEST_CASE("optional.cpp", "[scanner][stl]")
{
  const std::string snapshot_name = "stl-optional.db";

  ScannerInvocation inv{
    { "--compile-commands", STL_BUILD_DIR + std::string("/compile_commands.json"),
    "--home", STL_ROOT_DIR,
    "-f:tu", "optional.cpp",
    "--overwrite",
    "-o", snapshot_name }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  TemporarySnapshot s{ snapshot_name };

  std::vector<File> files = s.getFiles();
  REQUIRE(files.size() > 0);
  File optionalcpp = getFile(files, std::regex("optional\\.cpp"));
}

TEST_CASE("variant.cpp", "[scanner][stl]")
{
  const std::string snapshot_name = "stl-variant.db";

  ScannerInvocation inv{
    { "--compile-commands", STL_BUILD_DIR + std::string("/compile_commands.json"),
    "--home", STL_ROOT_DIR,
    "-f:tu", "variant.cpp",
    "--overwrite",
    "-o", snapshot_name }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  TemporarySnapshot s{ snapshot_name };

  std::vector<File> files = s.getFiles();
  REQUIRE(files.size() > 0);
  File variantcpp = getFile(files, std::regex("variant\\.cpp"));
}

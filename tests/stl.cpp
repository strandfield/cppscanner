
#include "helpers.h"
#include "projects.h"

#include "cppscanner/scannerInvocation/scannerinvocation.h"

#include "catch.hpp"

using namespace cppscanner;

static const std::string HOME_DIR = TESTFILES_DIRECTORY + std::string("/stl");

TEST_CASE("string.cpp", "[scanner][stl]")
{
  const std::string snapshot_name = "stl-string.db";

  ScannerInvocation inv{
    { "-i", HOME_DIR + std::string("/string.cpp"),
      "--home", HOME_DIR,
      "--index-local-symbols",
      "--overwrite",
      "-o", snapshot_name }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  TestSnapshotReader s{ snapshot_name };

  std::vector<File> files = s.getFiles();
  REQUIRE(files.size() > 0);
  File stringcpp = getFile(files, std::regex("string\\.cpp"));
  REQUIRE_THROWS(getFile(files, std::regex("vector\\.cpp")));

  SymbolRecord init = s.getSymbolByName("init()");
  REQUIRE(init.kind == SymbolKind::Function);

  SymbolRecord stdns = s.getSymbolByName("std");
  REQUIRE(stdns.kind == SymbolKind::Namespace);
  REQUIRE(!testFlag(stdns, SymbolFlag::FromProject));

  SymbolRecord symbol = s.getSymbolByName("string");
  REQUIRE((symbol.kind == SymbolKind::TypeAlias || symbol.kind == SymbolKind::Typedef));
  REQUIRE(!testFlag(symbol, SymbolFlag::FromProject));
  REQUIRE(symbol.parentId == stdns.id);

  symbol = s.getSymbolByName("basic_string");
  REQUIRE(symbol.kind == SymbolKind::Class);
  REQUIRE(!testFlag(symbol, SymbolFlag::FromProject));
  REQUIRE(symbol.parentId == stdns.id);
}

TEST_CASE("vector.cpp", "[scanner][stl]")
{
  const std::string snapshot_name = "stl-vector.db";

  ScannerInvocation inv{
    { "-i", HOME_DIR + std::string("/vector.cpp"),
    "--home", HOME_DIR,
    "--index-local-symbols",
    "--overwrite",
    "-o", snapshot_name }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  TestSnapshotReader s{ snapshot_name };

  std::vector<File> files = s.getFiles();
  REQUIRE(files.size() > 0);
  File vectorcpp = getFile(files, std::regex("vector\\.cpp"));
  REQUIRE_THROWS(getFile(files, std::regex("string\\.cpp")));

  SymbolRecord sortThisVector = s.getSymbolByName("sortThisVector(std::vector<int>&)");
  REQUIRE(sortThisVector.kind == SymbolKind::Function);
}

TEST_CASE("tuple.cpp", "[scanner][stl]")
{
  const std::string snapshot_name = "stl-tuple.db";

  ScannerInvocation inv{
    { "-i", HOME_DIR + std::string("/tuple.cpp"),
    "--home", HOME_DIR,
    "--index-local-symbols",
    "--overwrite",
    "-o", snapshot_name,
    "--", "-std=c++17" }
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

  SymbolRecord record = s.getSymbolByName("tuple");
  REQUIRE(record.kind == SymbolKind::Class);

  record = s.getSymbolByName("hello_tuple()");
  REQUIRE(record.kind == SymbolKind::Function);

  record = getRecord<FunctionRecord>(s, SymbolRecordFilter().withNameLike("make_tuple%"));
  REQUIRE(record.kind == SymbolKind::Function);
}

TEST_CASE("optional.cpp", "[scanner][stl]")
{
  const std::string snapshot_name = "stl-optional.db";

  ScannerInvocation inv{
    { "-i", HOME_DIR + std::string("/optional.cpp"),
    "--home", HOME_DIR,
    "--index-local-symbols",
    "--overwrite",
    "-o", snapshot_name,
    "--", "-std=c++17" }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  TestSnapshotReader s{ snapshot_name };

  std::vector<File> files = s.getFiles();
  REQUIRE(files.size() > 0);
  File optionalcpp = getFile(files, std::regex("optional\\.cpp"));

  SymbolRecord record = s.getSymbolByName("optional");
  REQUIRE(record.kind == SymbolKind::Class);

  record = s.getSymbolByName("hello_optional(int, std::optional<int>)");
  REQUIRE(record.kind == SymbolKind::Function);
  std::vector<ParameterRecord> params = s.getFunctionParameters(record.id);
  REQUIRE(params.size() == 2);
  REQUIRE(params.front().name == "a");
  REQUIRE(params.back().name == "b");
}

TEST_CASE("variant.cpp", "[scanner][stl]")
{
  const std::string snapshot_name = "stl-variant.db";

  ScannerInvocation inv{
    { "-i", HOME_DIR + std::string("/variant.cpp"),
    "--home", HOME_DIR,
    "--index-local-symbols",
    "--overwrite",
    "-o", snapshot_name,
    "--", "-std=c++17" }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  TestSnapshotReader s{ snapshot_name };

  std::vector<File> files = s.getFiles();
  REQUIRE(files.size() > 0);
  File variantcpp = getFile(files, std::regex("variant\\.cpp"));

  SymbolRecord record = s.getSymbolByName("variant");
  REQUIRE(record.kind == SymbolKind::Class);

  record = s.getSymbolByName("hello_variant(const std::variant<char, bool, int, double>&)");
  REQUIRE(record.kind == SymbolKind::Function);
  std::vector<ParameterRecord> params = s.getFunctionParameters(record.id);
  REQUIRE(params.size() == 1);
  REQUIRE(params.front().name == "value");

  record = s.getSymbolByName("VariantVisitor");
  REQUIRE(record.kind == SymbolKind::Struct);
  
  {
    auto members = fetchAll<FunctionRecord>(s, SymbolRecordFilter().ofKind(SymbolKind::Operator).withNameLike("operator%"));
    REQUIRE(members.size() == 4);
  }
}

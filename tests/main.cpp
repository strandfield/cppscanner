
#include "helpers.h"
#include "projects.h"

#include "cppscanner/scannerInvocation/scannerinvocation.h"
#include "cppscanner/index/symbol.h"
#include "cppscanner/indexer/fileindexingarbiter.h"
#include "cppscanner/base/glob.h"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

using namespace cppscanner;


TEST_CASE("SymbolKind", "[index]")
{
  REQUIRE(getSymbolKindString(SymbolKind::Enum) == "enum");
  REQUIRE(getSymbolKindString(SymbolKind::EnumConstant) == "enum-constant");
}


TEST_CASE("Matching files", "[glob]")
{
  REQUIRE(!is_glob_pattern("myfile.cpp"));
  REQUIRE(filename_match("~/directory/myfile.cpp", "myfile.cpp"));
  REQUIRE(!filename_match("~/directory/myfile.h", "myfile.cpp"));

  REQUIRE(is_glob_pattern("directory"));
  REQUIRE(glob_match("~/directory/myfile.h", "directory"));
  REQUIRE(is_glob_pattern("directory/"));
  REQUIRE(glob_match("~/directory/myfile.h", "directory/"));

  REQUIRE(is_glob_pattern("myfile.*"));
  REQUIRE(glob_match("~/directory/myfile.cpp", "myfile.*"));
  REQUIRE(glob_match("~/directory/myfile.h", "myfile.*"));

  REQUIRE(is_glob_pattern("/t?t?.*"));
  REQUIRE(glob_match("~/directory/titi.cpp", "/t?t?.*"));
  REQUIRE(glob_match("~/directory/toto.h", "/t?t?.*"));

#ifdef WIN32
  REQUIRE(glob_match("a\\b", "a/b"));
  REQUIRE(glob_match("a\\b", "a\\b"));
  REQUIRE(glob_match("a/b", "a\\b"));
  REQUIRE(glob_match("a/b", "a/b"));
#endif // WIN32

}

TEST_CASE("jobs", "[scannerInvocation]")
{
  ScannerInvocation inv{
    { "run",
    "-i", "test.cpp",
    "-j8",
    "--project-name", "cppscanner",
    "-o", "output.db"}
  };

  REQUIRE(std::holds_alternative<ScannerInvocation::RunOptions>(inv.options().command));
  ScannerInvocation::RunOptions opts = std::get<ScannerInvocation::RunOptions>(inv.options().command);

  REQUIRE(opts.nb_threads.value_or(-1) == 8);
  REQUIRE(opts.project_name.value_or("") == "cppscanner");
  REQUIRE(opts.output == "output.db");
  REQUIRE(opts.inputs.size() == 1);
  REQUIRE(opts.inputs.at(0) == "test.cpp");
}

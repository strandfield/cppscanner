
#include "helpers.h"
#include "projects.h"

#include "cppscanner/scannerInvocation/scannerinvocation.h"
#include "cppscanner/index/symbol.h"
#include "cppscanner/indexer/glob.h"
#include "cppscanner/indexer/fileindexingarbiter.h"

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

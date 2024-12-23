
#include "helpers.h"
#include "projects.h"

#include "cppscanner/indexer/symbolrecorditerator.h"
#include "cppscanner/scannerInvocation/scannerinvocation.h"

#include "catch.hpp"

using namespace cppscanner;

static const std::string HOME_DIR = TESTFILES_DIRECTORY + std::string("/cpp20_library_features");

TEST_CASE("Formatting library (C++20)", "[scanner][cpp20_library_features]")
{
  const std::string snapshot_name = "cpp20_library_features-format.db";

  ScannerInvocation inv{
    { "-i", HOME_DIR + std::string("/format.cpp"),
    "--home", HOME_DIR,
    "--overwrite",
    "-o", snapshot_name,
    "--", "-std=c++20"}
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  SnapshotReader s{ snapshot_name };

  SymbolRecord testfunc = s.getSymbolByName("testFormatLibrary()");
  REQUIRE(testfunc.kind == SymbolKind::Function);

  // check if <format> header is present
  {
    std::vector<SymbolRecord> symbols = s.getSymbolsByName("NO_FORMAT_HEADER");
    REQUIRE(symbols.size() <= 1);
    if (symbols.size() == 1)
    {
      SymbolRecord sym = symbols.front();
      REQUIRE(sym.kind == SymbolKind::Macro);
      return;
    }
  }

  SymbolRecord symbol = s.getSymbolByName("QuotableString");
  REQUIRE(symbol.kind == SymbolKind::Struct);
  
  std::vector<BaseOf> bases = s.getBasesOf(symbol.id);
  REQUIRE(bases.size() == 1);

  symbol = s.getSymbolById(bases.front().baseClassID);
  REQUIRE(symbol.name == "basic_string_view");

  symbol = s.getSymbolByName("parse(ParseContext&)");
  REQUIRE(symbol.kind == SymbolKind::Method);

  SymbolRecord formatter = s.getSymbolById(symbol.parentId);
  REQUIRE(formatter.kind == SymbolKind::Struct);
  REQUIRE(formatter.name == "formatter");

  SymbolRecord formatmethod = s.getSymbolByName("format(QuotableString, FmtContext&) const");
  REQUIRE(formatmethod.kind == SymbolKind::Method);
  REQUIRE(formatmethod.parentId == formatter.id);

  // find the record corresponding to the std::format() function
  SymbolRecord formatfun;
  {
    SymbolRecordIterator iterator{ s, SymbolRecordFilter().ofKind(SymbolKind::Function) };

    while (iterator.hasNext())
    {
      SymbolRecord sr = iterator.next();
      
      if (sr.name.rfind("format(", 0) == 0)
      {
        if (sr.id != formatmethod.id)
        {
          formatfun = std::move(sr);
          break;
         }
      }
    }
  }

  REQUIRE(formatfun.kind == SymbolKind::Function);

  // find & verify the 4 references to the std::format() function
  std::vector<SymbolReference> references = s.findReferences(formatfun.id);
  REQUIRE(references.size() == 4);
  for (SymbolReference r : references)
  {
    REQUIRE(testFlag(r, SymbolReference::Call));
    REQUIRE(r.referencedBySymbolID == testfunc.id);
  }
}

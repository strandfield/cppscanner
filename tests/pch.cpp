
#include "helpers.h"
#include "projects.h"

#include "cppscanner/cmakeIntegration/cmakeproject.h"
#include "cppscanner/scannerInvocation/cmakeinvocation.h"
#include "cppscanner/scannerInvocation/scannerinvocation.h"

#include "cppscanner/database/sql.h"

#include "catch.hpp"

using namespace cppscanner;

TEST_CASE("precompiled headers", "[scanner][pch]")
{
  const std::string snapshot_name = "precompiled_headers.db";
  SnapshotDeleter snapshot_deleter{ snapshot_name };

  ScannerInvocation inv{
    { "--compile-commands", PRECOMPILED_HEADERS_BUILD_DIR + std::string("/compile_commands.json"),
    "--home", PRECOMPILED_HEADERS_ROOT_DIR,
    "-o", snapshot_name }
  };

  REQUIRE_NOTHROW(inv.run());
  CHECK(inv.errors().empty());

  SnapshotReader s{ snapshot_name };

  std::vector<File> files = s.getFiles();
  File srcfile = getFile(files, std::regex("main\\.cpp"));
  File ah = getFile(files, std::regex("a\\.h"));
  File bh = getFile(files, std::regex("b\\.h"));
  File ch = getFile(files, std::regex("c\\.h"));

  std::vector<Diagnostic> diagnostics = s.getDiagnostics();
  REQUIRE(diagnostics.empty());

  {
    auto sayGoodbye = getRecord<FunctionRecord>(s, SymbolRecordFilter().withName("sayGoodBye()"));
    auto mainfunc = getRecord<FunctionRecord>(s, SymbolRecordFilter().withName("main()"));

    std::vector<SymbolReference> refs = s.findReferences(sayGoodbye.id);
    REQUIRE(refs.size() == 2);

    if (testFlag(refs.front(), SymbolReference::Call)) {
      std::swap(refs.front(), refs.back());
    }

    SymbolReference ref = refs.at(0);
    REQUIRE(testFlag(ref, SymbolReference::Definition));
    REQUIRE(ref.fileID == ah.id);

    ref = refs.at(1);
    REQUIRE(testFlag(ref, SymbolReference::Call));
    REQUIRE(ref.fileID == srcfile.id);
    REQUIRE(ref.referencedBySymbolID == mainfunc.id);
  }

  {
    auto myEnum = getRecord<EnumRecord>(s, SymbolRecordFilter().withName("MyEnum"));
    REQUIRE(myEnum.kind == SymbolKind::Enum);

    std::vector<SymbolDeclaration> decls = s.getSymbolDeclarations(myEnum.id);
    REQUIRE(decls.size() == 1);
    auto decl = decls.front();
    REQUIRE(decl.fileID == ch.id);

    auto children = s.getChildSymbols(myEnum.id, SymbolKind::EnumConstant);
    REQUIRE(children.size() == 3);
  }
}
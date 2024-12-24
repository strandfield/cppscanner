
#include "helpers.h"
#include "projects.h"

#include "cppscanner/scannerInvocation/scannerinvocation.h"
#include "cppscanner/indexer/argumentsadjuster.h"

#include "catch.hpp"

using namespace cppscanner;

TEST_CASE("modules", "[scanner][cpp20_modules]")
{
  const std::string snapshot_name = "cpp20_modules.db";

  constexpr const char* cpp20_modules_dir = "cpp20_modules";
  const auto maincppPath = std::filesystem::path(TESTFILES_DIRECTORY) / cpp20_modules_dir / "main.cpp";
  const auto hellocppmPath = std::filesystem::path(TESTFILES_DIRECTORY) / cpp20_modules_dir / "Hello.cppm";
  REQUIRE(std::filesystem::exists(maincppPath));
  REQUIRE(std::filesystem::exists(hellocppmPath));

  {
    Scanner scanner;
    scanner.setHomeDir(std::filesystem::path(TESTFILES_DIRECTORY) / cpp20_modules_dir);
    scanner.setIndexLocalSymbols(true);

    if (std::filesystem::exists(snapshot_name))
    {
      std::filesystem::remove(snapshot_name);
    }
    scanner.initSnapshot(snapshot_name);

    std::vector<ScannerCompileCommand> commands;
    {
      auto adjuster = getPchArgumentsAdjuster();
      ScannerCompileCommand cmd;
      cmd.fileName = hellocppmPath.generic_u8string();
      cmd.commandLine = { "clang++","-std=c++20", hellocppmPath.generic_u8string(), "--precompile", "-o", "Hello.pcm" };
      cmd.commandLine = adjuster(cmd.commandLine, cmd.fileName);
      commands.push_back(cmd);
    }
    {
      auto adjuster = getDefaultArgumentsAdjuster();
      ScannerCompileCommand cmd;
      cmd.fileName = maincppPath.generic_u8string();
      cmd.commandLine = { "clang++","-std=c++20", maincppPath.generic_u8string(), "-fmodule-file=Hello=Hello.pcm", "Hello.pcm", "-o", "Hello.out" };
      cmd.commandLine = adjuster(cmd.commandLine, cmd.fileName);
      commands.push_back(cmd);
    }

    scanner.scan(commands);
  }

  SnapshotReader s{ snapshot_name };

  std::vector<File> files = s.getFiles();
  File maincpp = getFile(files, std::regex("main\\.cpp"));

  SymbolRecord mymodule = s.getSymbolByName("Hello");
  REQUIRE(mymodule.kind == SymbolKind::Module);

  std::vector<SymbolReference> refs = s.findReferences(mymodule.id);
  REQUIRE(refs.size() == 1);
  SymbolReference r = refs.at(0);
  REQUIRE(r.fileID == maincpp.id);
  REQUIRE(r.position.line() == 2);
}


#include "helpers.h"
#include "projects.h"

#include "cppscanner/indexer/scanner.h"

#include "catch.hpp"

using namespace cppscanner;

TEST_CASE("modules", "[scanner][cpp20_modules]")
{
  const std::string snapshot_name = "cpp20_modules.db";
  SnapshotDeleter snapshot_deleter{ snapshot_name };

  constexpr const char* cpp20_modules_dir = "cpp20_modules";
  const auto maincppPath = std::filesystem::path(TESTFILES_DIRECTORY) / cpp20_modules_dir / "main.cpp";
  const auto hellocppmPath = std::filesystem::path(TESTFILES_DIRECTORY) / cpp20_modules_dir / "Hello.cppm";
  REQUIRE(std::filesystem::exists(maincppPath));
  REQUIRE(std::filesystem::exists(hellocppmPath));

  {
    Scanner scanner;
    scanner.setOutputPath(snapshot_name);
    scanner.setHomeDir(std::filesystem::path(TESTFILES_DIRECTORY) / cpp20_modules_dir);
    scanner.setIndexLocalSymbols(true);

    std::vector<CompileCommand> commands;
    {
      CompileCommand cmd;
      cmd.fileName = hellocppmPath.generic_u8string();
      cmd.commandLine = { "clang++","-std=c++20", hellocppmPath.generic_u8string(), "--precompile", "-o", "Hello.pcm" };
      commands.push_back(cmd);
    }
    {
      CompileCommand cmd;
      cmd.fileName = maincppPath.generic_u8string();
      cmd.commandLine = { "clang++","-std=c++20", maincppPath.generic_u8string(), "-fmodule-file=Hello=Hello.pcm", "Hello.pcm", "-o", "Hello.out" };
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

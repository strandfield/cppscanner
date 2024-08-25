
#include "helpers.h"
#include "projects.h"

#include "cppscanner/scannerInvocation/scannerinvocation.h"

#include "catch.hpp"

using namespace cppscanner;

TEST_CASE("The Scanner runs properly on cxx_language_features", "[scanner][cxx_language_features]")
{
  const std::string snapshot_name = "cxx_language_features.db";

  ScannerInvocation inv{
    {"--compile-commands", CXX_LANGUAGE_FEATURES_BUILD_DIR + std::string("/compile_commands.json"), "--index-external-files", "--index-local-symbols", "--overwrite", "-o", snapshot_name}
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  auto s = Snapshot::open(snapshot_name);

  std::vector<File> files = s.getFiles();
  REQUIRE(files.size() == 4);
  File maincpp = getFile(files, std::regex("main\\.cpp"));
  File lambdacpp = getFile(files, std::regex("lambda\\.cpp"));

  // lambda
  {
    Symbol fwithlambda = s.getSymbolByName("functionWithLambda");
    std::vector<Symbol> alllambdas = s.getSymbols(fwithlambda.id, SymbolKind::Lambda);
    REQUIRE(alllambdas.size() == 1);
    const Symbol& lambda = alllambdas.front();
    REQUIRE(!lambda.name.empty());
    REQUIRE(lambda.name.rfind("__lambda_", 0) != std::string::npos);

    Symbol add_to_a = s.getSymbol({ "functionWithLambda", "add_to_a" });
    REQUIRE(add_to_a.kind == SymbolKind::Variable);
  }

  // enum
  {
    Symbol enumclass = s.getSymbol({ "cxx", "EnumClass" });
    REQUIRE(enumclass.testFlag(Symbol::IsScoped));

    Symbol enumclass_z = s.getSymbol({ "cxx", "EnumClass", "Z" });
    REQUIRE(enumclass_z.value == "25");
  }
  
  // function template with type parameter
  {
    Symbol incr = s.getSymbol({ "max" });
    REQUIRE(incr.kind == SymbolKind::Function);
    std::vector<Symbol> ttps = s.getSymbols(incr.id, SymbolKind::TemplateTypeParameter);
    REQUIRE(ttps.size() == 1);
    Symbol T = ttps.front();
    REQUIRE(T.name == "T");
    REQUIRE(T.parameterIndex == 0);
  }

  // function template with non-type parameter
  {
    Symbol incr = s.getSymbol({ "incr" });
    REQUIRE(incr.kind == SymbolKind::Function);
    std::vector<Symbol> ntps = s.getSymbols(incr.id, SymbolKind::NonTypeTemplateParameter);
    REQUIRE(ntps.size() == 1);
    Symbol N = ntps.front();
    REQUIRE(N.name == "N");
    REQUIRE(N.parameterIndex == 0);
    REQUIRE(N.type == "int");
  }

  // class template with a specialization
  {
    std::vector<Symbol> symbols = s.findSymbolsByName("is_same");
    REQUIRE(symbols.size() == 2);
    Symbol base = symbols.front();
    Symbol spec = symbols.back();

    symbols = s.getSymbols(base.id, SymbolKind::TemplateTypeParameter);
    if (symbols.size() != 2)
    {
      std::swap(base, spec);
      symbols = s.getSymbols(base.id, SymbolKind::TemplateTypeParameter);
    }

    REQUIRE(symbols.size() == 2);
    {
      symbols = s.getSymbols(base.id, SymbolKind::StaticProperty);
      REQUIRE(symbols.size() == 1);
      Symbol value = symbols.front();
      REQUIRE(value.name == "value");
      REQUIRE(value.type == "const bool");
      REQUIRE(value.value == "false");
    }

    symbols = s.getSymbols(spec.id, SymbolKind::TemplateTypeParameter);
    REQUIRE(symbols.size() == 1);
    REQUIRE(symbols.front().name == "T");
    {
      symbols = s.getSymbols(spec.id, SymbolKind::StaticProperty);
      REQUIRE(symbols.size() == 1);
      Symbol value = symbols.front();
      REQUIRE(value.name == "value");
      REQUIRE(value.type == "const bool");
      REQUIRE(value.value == "true");
    }
  }
}

TEST_CASE("Preprocessor macros", "[scanner][cxx_language_features]")
{
  const std::string snapshot_name = "cxx_language_features-macro.db";

  ScannerInvocation inv{
    { "--compile-commands", CXX_LANGUAGE_FEATURES_BUILD_DIR + std::string("/compile_commands.json"),
    "--home", CXX_LANGUAGE_FEATURES_ROOT_DIR,
    "-f:tu", "macro.cpp",
    "--overwrite",
    "-o", snapshot_name }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  TemporarySnapshot s{ snapshot_name };
  s.delete_on_close = false;

  std::vector<File> files = s.getFiles();
  REQUIRE(files.size() > 0);
  File macro_cpp = getFile(files, std::regex("macro\\.cpp"));

  Symbol my_constant = s.getSymbolByName("MY_CONSTANT");
  Symbol greater_than_my_constant = s.getSymbolByName("GREATER_THAN_MY_CONSTANT");
  REQUIRE(my_constant.kind == SymbolKind::Macro);
  REQUIRE(greater_than_my_constant.kind == SymbolKind::Macro);

  std::vector<Symbol> my_mins = s.findSymbolsByName("MY_MIN");
  REQUIRE(my_mins.size() == 2);
  Symbol my_min0 = my_mins[0];
  Symbol my_min1 = my_mins[1];
  REQUIRE(my_min0.kind == SymbolKind::Macro);
  REQUIRE(my_min1.kind == SymbolKind::Macro);

 // REQUIRE_THROWS(s.getSymbolByName("MY_GUARD"));
}

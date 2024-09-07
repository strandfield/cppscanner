
#include "helpers.h"
#include "projects.h"

#include "cppscanner/scannerInvocation/scannerinvocation.h"

#include "cppscanner/database/sql.h"

#include "catch.hpp"

using namespace cppscanner;

static EnumConstantRecord getEnumConstantRecord(const SnapshotReader& s, SymbolID enumId, const std::string& name)
{
  return cppscanner::getRecord<EnumConstantRecord>(s, SymbolRecordFilter().withParent(enumId).withName(name));
}

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

  SnapshotReader s{ snapshot_name };

  std::vector<File> files = s.getFiles();
  REQUIRE(files.size() == 8);
  File maincpp = getFile(files, std::regex("main\\.cpp"));
  File lambdacpp = getFile(files, std::regex("lambda\\.cpp"));

  // lambda
  {
    SymbolRecord fwithlambda = s.getChildSymbolByName("functionWithLambda(int&, int)");
    std::vector<SymbolRecord> alllambdas = s.getChildSymbols(fwithlambda.id, SymbolKind::Lambda);
    REQUIRE(alllambdas.size() == 1);
    const SymbolRecord& lambda = alllambdas.front();
    REQUIRE(!lambda.name.empty());
    REQUIRE(lambda.name.rfind("__lambda_", 0) != std::string::npos);

    SymbolRecord add_to_a = s.getSymbolByName({ "functionWithLambda(int&, int)", "add_to_a" });
    REQUIRE(add_to_a.kind == SymbolKind::Variable);
  }

  // enum
  {
    SymbolRecord enumclass = s.getSymbolByName({ "cxx", "EnumClass" });
    REQUIRE(testFlag(enumclass, EnumInfo::IsScoped));

    EnumConstantRecord enumclass_z = getEnumConstantRecord(s, enumclass.id, "Z");
    REQUIRE(enumclass_z.expression == "25");
  }
  
  // function template with type parameter
  {
    SymbolRecord incr = getRecord(s, SymbolRecordFilter().withNameLike("max(%)"));
    REQUIRE(incr.kind == SymbolKind::Function);
    std::vector<ParameterRecord> ttps = s.getFunctionParameters(incr.id, SymbolKind::TemplateTypeParameter);
    REQUIRE(ttps.size() == 1);
    ParameterRecord T = ttps.front();
    REQUIRE(T.name == "T");
    REQUIRE(T.parameterIndex == 0);
  }

  // function template with non-type parameter
  {
    SymbolRecord incr = getRecord(s, SymbolRecordFilter().withNameLike("incr(%)"));
    REQUIRE(incr.kind == SymbolKind::Function);
    std::vector<ParameterRecord> ntps = s.getFunctionParameters(incr.id, SymbolKind::NonTypeTemplateParameter);
    REQUIRE(ntps.size() == 1);
    ParameterRecord N = ntps.front();
    REQUIRE(N.name == "N");
    REQUIRE(N.parameterIndex == 0);
    REQUIRE(N.type == "int");
  }

  // class template with a specialization
  {
    std::vector<SymbolRecord> symbols = s.getSymbolsByName("is_same");
    REQUIRE(symbols.size() == 2);
    SymbolRecord base = symbols.front();
    SymbolRecord spec = symbols.back();

    symbols = s.getChildSymbols(base.id, SymbolKind::TemplateTypeParameter);
    if (symbols.size() != 2)
    {
      std::swap(base, spec);
      symbols = s.getChildSymbols(base.id, SymbolKind::TemplateTypeParameter);
    }

    REQUIRE(symbols.size() == 2);
    {
      std::vector<VariableRecord> properties = s.getStaticProperties(base.id);
      REQUIRE(properties.size() == 1);
      VariableRecord value = properties.front();
      REQUIRE(value.name == "value");
      REQUIRE(value.type == "const bool");
      REQUIRE(value.init == "false");
    }

    symbols = s.getChildSymbols(spec.id, SymbolKind::TemplateTypeParameter);
    REQUIRE(symbols.size() == 1);
    REQUIRE(symbols.front().name == "T");
    {
      std::vector<VariableRecord> properties = s.getStaticProperties(spec.id);
      REQUIRE(properties.size() == 1);
      VariableRecord value = properties.front();
      REQUIRE(value.name == "value");
      REQUIRE(value.type == "const bool");
      REQUIRE(value.init == "true");
    }
  }
}

static MacroRecord getMacroRecordByName(const SnapshotReader& s, const std::string& name)
{
  return cppscanner::getRecord<MacroRecord>(s, SymbolRecordFilter().withName(name));
}

static MacroRecord getMacroRecords(const SnapshotReader& s, const std::string& name)
{
  return cppscanner::getRecord<MacroRecord>(s, SymbolRecordFilter().withName(name));
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

  std::vector<File> files = s.getFiles();
  REQUIRE(files.size() > 0);
  File macro_cpp = getFile(files, std::regex("macro\\.cpp"));

  MacroRecord my_constant = getMacroRecordByName(s, "MY_CONSTANT");
  MacroRecord greater_than_my_constant = getMacroRecordByName(s, "GREATER_THAN_MY_CONSTANT");
  REQUIRE(my_constant.kind == SymbolKind::Macro);
  REQUIRE(greater_than_my_constant.kind == SymbolKind::Macro);

  std::vector<MacroRecord> my_mins = fetchAll<MacroRecord>(s, SymbolRecordFilter().withName("MY_MIN"));
  REQUIRE(my_mins.size() == 2);
  MacroRecord my_min0 = my_mins[0];
  MacroRecord my_min1 = my_mins[1];
  REQUIRE(my_min0.kind == SymbolKind::Macro);
  REQUIRE(my_min1.kind == SymbolKind::Macro);

  if (my_min0.definition == "min_int(a, b)") {
    std::swap(my_min0, my_min1);
  }

  REQUIRE(my_min0.definition == "( (a) < (b) ? (a) : (b) )");
  REQUIRE(my_min1.definition == "min_int(a, b)");
  REQUIRE(greater_than_my_constant.definition == "(MY_MIN(MY_CONSTANT, (a)) == MY_CONSTANT)");

  MacroRecord my_guard = getMacroRecordByName(s, "MY_GUARD");
  REQUIRE(testFlag(my_guard, MacroInfo::MacroUsedAsHeaderGuard));
}

TEST_CASE("Namespaces", "[scanner][cxx_language_features]")
{
  const std::string snapshot_name = "cxx_language_features-namespace.db";

  ScannerInvocation inv{
    { "--compile-commands", CXX_LANGUAGE_FEATURES_BUILD_DIR + std::string("/compile_commands.json"),
    "--home", CXX_LANGUAGE_FEATURES_ROOT_DIR,
    "-f:tu", "namespace.cpp",
    "--overwrite",
    "-o", snapshot_name }
  };

  // the scanner invocation succeeds
  {
    REQUIRE_NOTHROW(inv.run());
    REQUIRE(inv.errors().empty());
  }

  TemporarySnapshot s{ snapshot_name };

  SymbolRecord namespaceA = s.getChildSymbolByName("namespaceA");
  SymbolRecord namespaceB = s.getChildSymbolByName("namespaceB");

  NamespaceAliasRecord nsA = s.getNamespaceAliasRecord("nsA");
  NamespaceAliasRecord nsB = s.getNamespaceAliasRecord("nsB");
  REQUIRE(nsA.value == "namespaceA");
  REQUIRE(nsB.value == "nsA::namespaceB");

  SymbolRecord inlNs = s.getChildSymbolByName("inlineNamespace");
  REQUIRE(testFlag(inlNs, NamespaceInfo::Inline));
}

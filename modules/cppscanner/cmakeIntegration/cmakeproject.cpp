
#include "cmakeproject.h"

#include <llvm/Support/JSON.h>

#include <cassert>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <optional>
#include <sstream>

namespace cppscanner
{

namespace
{

std::optional<std::filesystem::path> find_index_json(const std::filesystem::path& path)
{
  assert(std::filesystem::is_directory(path));

  for (const auto& entry : std::filesystem::directory_iterator(path))
  {
    if (!entry.is_regular_file()) {
      continue;
    }

    if (entry.path().extension() != ".json") {
      continue;
    }

    std::string filename = entry.path().filename().string();

    if (filename.rfind("index", 0) == 0)
    {
      return entry.path();
    }
  }

  return std::nullopt;
}

std::string read_all(const std::filesystem::path& path)
{
  std::ifstream stream{ path };
  std::stringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

const llvm::json::Object* getObject(const llvm::json::Value* val, std::initializer_list<std::string>&& keys)
{
  const llvm::json::Object* obj = val->getAsObject();

  for (const std::string& k : keys)
  {
    obj = obj ? obj->getObject(k) : nullptr;
  }

  return obj;
}

void parse_target(CMakeIndex& idx, CMakeTarget& target, const std::string& jsonFile)
{
  const std::filesystem::path jsonPath = idx.indexJsonFile.parent_path() / jsonFile;
  std::string content = read_all(jsonPath);
  llvm::Expected<llvm::json::Value> parse_result = llvm::json::parse(content);
  if (!parse_result) {
    return;
  }
  content.clear();

  const llvm::json::Value& rootValue = *parse_result;
  const llvm::json::Object* rootObject = rootValue.getAsObject();

  if (!rootObject) {
    return;
  }

  target.type = rootObject->getString("type").value_or(std::string()).str();

  if (const llvm::json::Array* dependencies = rootObject->getArray("dependencies"))
  {
    for (const llvm::json::Value& depValue : *dependencies)
    {
      const llvm::json::Object* depObject = depValue.getAsObject();

      if (!depObject) {
        continue;
      }

      std::optional<llvm::StringRef> id = depObject->getString("id");

      if (id.has_value()) {
        target.dependencies.push_back(id->str());
      }
    }
  }

  if (const llvm::json::Array* sources = rootObject->getArray("sources"))
  {
    for (const llvm::json::Value& sourceValue : *sources)
    {
      const llvm::json::Object* sourceObj = sourceValue.getAsObject();

      if (!sourceObj) {
        continue;
      }

      std::optional<llvm::StringRef> path = sourceObj->getString("path");

      if (path.has_value()) {
        target.sources.push_back(path->str());
      }
    }
  }

  if (const llvm::json::Array* compileGroups = rootObject->getArray("compileGroups"))
  {
    for (const llvm::json::Value& compileGroupValue : *compileGroups)
    {
      const llvm::json::Object* compileGroupObj = compileGroupValue.getAsObject();

      if (!compileGroupObj) {
        continue;
      }

      CMakeTarget::CompileGroup compileGroup;

      compileGroup.language = compileGroupObj->getString("language").value_or(std::string()).str();

      //if (lang == "CXX")
      //{
      //  compileGroup.language = CMakeTarget::CompileGroup::LANG_CXX;
      //}
      //else if (lang == "C")
      //{
      //  compileGroup.language = CMakeTarget::CompileGroup::LANG_C;
      //}

      if (const llvm::json::Object* languageStandardObj = compileGroupObj->getObject("languageStandard"))
      {
        compileGroup.languageStandard = languageStandardObj->getString("standard").value_or(std::string()).str();
        
        /*if (standard == "14")
        {
          compileGroup.languageStandard = CMakeTarget::CompileGroup::STD_CPP14;
        }
        else if (standard == "17")
        {
          compileGroup.languageStandard = CMakeTarget::CompileGroup::STD_CPP17;
        }
        else if (standard == "20")
        {
          compileGroup.languageStandard = CMakeTarget::CompileGroup::STD_CPP20;
        }
        else if (standard == "23")
        {
          compileGroup.languageStandard = CMakeTarget::CompileGroup::STD_CPP23;
        }
        else if (standard == "26")
        {
          compileGroup.languageStandard = CMakeTarget::CompileGroup::STD_CPP26;
        }
        else
        {
          compileGroup.languageStandard = CMakeTarget::CompileGroup::STD_CPP11;
        }*/
      }

      if (const llvm::json::Array* compileCommandFragments = compileGroupObj->getArray("compileCommandFragments"))
      {
        for (const llvm::json::Value& fragValue : *compileCommandFragments)
        {
          const llvm::json::Object* fragObj = fragValue.getAsObject();

          if (!fragObj) {
            continue;
          }

          CMakeTarget::CompileCommandFragment frag;
          frag.fragment = fragObj->getString("fragment").value_or(std::string()).str();

          if (!frag.fragment.empty()) {
            compileGroup.compileCommandFragments.push_back(std::move(frag));
          }
        }
      }

      if (const llvm::json::Array* sourceIndexes = compileGroupObj->getArray("sourceIndexes"))
      {
        for (const llvm::json::Value& val : *sourceIndexes)
        {
          auto index = val.getAsInteger();

          if (index.has_value())
          {
            compileGroup.sourceIndexes.push_back(static_cast<int>(*index));
          }
        }
      }

      target.compileGroups.push_back(std::move(compileGroup));
    }
  }
}

void parse_configuration(CMakeIndex& idx, const llvm::json::Value& rootValue)
{
  const llvm::json::Object* rootObject = rootValue.getAsObject();

  if (!rootObject) {
    return;
  }

  CMakeConfiguration result;
  result.name = rootObject->getString("name").value_or(std::string()).str();

  if (const llvm::json::Array* projects = rootObject->getArray("projects"))
  {
    for (const llvm::json::Value& proValue : *projects)
    {
      const llvm::json::Object* proObj = proValue.getAsObject();

      if (!proObj) {
        continue;
      }

      CMakeProject pro;
      pro.name = proObj->getString("name").value_or(std::string()).str();

      if (const llvm::json::Array* targetIndexes = proObj->getArray("targetIndexes"))
      {
        for (const llvm::json::Value& targetValue : *targetIndexes)
        {
          std::optional<int64_t> i = targetValue.getAsInteger();

          if (i.has_value()) {
            pro.targetIndexes.push_back(static_cast<int>(*i));
          }
        }
      }

      result.projects.push_back(std::move(pro));
    }
  }

  if (const llvm::json::Array* targets = rootObject->getArray("targets"))
  {
    for (const llvm::json::Value& targetValue : *targets)
    {
      const llvm::json::Object* targetObj = targetValue.getAsObject();

      if (!targetObj) {
        continue;
      }

      std::optional<llvm::StringRef> id = targetObj->getString("id");
      std::optional<llvm::StringRef> name = targetObj->getString("name");

      if (!id || !name) {
        continue;
      }

      CMakeTarget target{ id.value().str(), name.value().str() };

      target.projectIndex = targetObj->getInteger("projectIndex").value_or(-1);

      std::optional<llvm::StringRef> jsonFile = targetObj->getString("jsonFile");

      if (jsonFile.has_value())
      {
        parse_target(idx, target, jsonFile->str());
      }

      result.targets.push_back(std::move(target));
    }
  }

  idx.configurations.push_back(std::move(result));
}

bool parse_codemodel(CMakeIndex& idx, const std::string& jsonFile)
{
  const std::filesystem::path jsonPath = idx.indexJsonFile.parent_path() / jsonFile;

  std::string content = read_all(jsonPath);

  llvm::Expected<llvm::json::Value> parse_result = llvm::json::parse(content);

  if (!parse_result) {
    return false;
  }

  content.clear();

  const llvm::json::Value& rootValue = *parse_result;
  const llvm::json::Object* rootObject = rootValue.getAsObject();

  if (!rootObject) {
    return false;
  }

  {
    const llvm::json::Object* paths = rootObject->getObject("paths");

    if (paths)
    {
      idx.paths.build = paths->getString("build").value_or(std::string()).str();
      idx.paths.source = paths->getString("source").value_or(std::string()).str();
    }
  }

  {
    const llvm::json::Array* configurations = rootObject->getArray("configurations");

    if (configurations)
    {
      for (const llvm::json::Value& conf : *configurations)
      {
        parse_configuration(idx, conf);
      }
    }
  }

  return true;
}

void parse_toolchain(CMakeIndex& idx, const llvm::json::Value& toolchainVal)
{
  const llvm::json::Object* toolchainObj = toolchainVal.getAsObject();

  if (!toolchainObj) {
    return;
  }

  CMakeToolchain toolchain;
  toolchain.language = toolchainObj->getString("language").value_or(std::string()).str();

  if (const llvm::json::Object* compilerObj = toolchainObj->getObject("compiler"))
  {
    toolchain.compiler.id = compilerObj->getString("id").value_or(std::string()).str();
    toolchain.compiler.version = compilerObj->getString("version").value_or(std::string()).str();
    toolchain.compiler.path = compilerObj->getString("path").value_or(std::string()).str();

    if (const llvm::json::Object* implicitObj = compilerObj->getObject("implicit"))
    {
      //TODO
    }
  }

  idx.toolchains.push_back(std::move(toolchain));
}

bool parse_toolchains(CMakeIndex& idx, const std::string& jsonFile)
{
  const std::filesystem::path jsonPath = idx.indexJsonFile.parent_path() / jsonFile;

  std::string content = read_all(jsonPath);

  llvm::Expected<llvm::json::Value> parse_result = llvm::json::parse(content);

  if (!parse_result) {
    return false;
  }

  content.clear();

  const llvm::json::Value& rootValue = *parse_result;
  const llvm::json::Object* rootObject = rootValue.getAsObject();

  if (!rootObject) {
    return false;
  }

  if (const llvm::json::Array* toolchains = rootObject->getArray("toolchains"))
  {
    for (const llvm::json::Value& toolchainVal : *toolchains)
    {
      parse_toolchain(idx, toolchainVal);
    }
  }

  return true;
}

} // namespace

bool CMakeIndex::read(const std::filesystem::path& path)
{
  if (std::filesystem::is_directory(path))
  {
    if (std::filesystem::exists(path / "CMakeCache.txt"))
    {
      return read(path / ".cmake" / "api" / "v1" / "reply");
    }

    std::optional<std::filesystem::path> indexjson = find_index_json(path);

    if (!indexjson.has_value())
    {
      return false;
    }

    std::cout << "Reading cmake index " << indexjson->string() << std::endl;

    return read(*indexjson);
  }

  const std::string content = read_all(path);

  llvm::Expected<llvm::json::Value> parse_result = llvm::json::parse(content);

  if (!parse_result) {
    return false;
  }

  indexJsonFile = path;

  llvm::json::Value json = *parse_result;
  const llvm::json::Object* query = getObject(&json, { "reply", "client-cppscanner", "query.json" });
  const llvm::json::Array* responses = query ? query->getArray("responses") : nullptr;

  if (!responses) {
    return false;
  }

  for (const llvm::json::Value& res : *responses)
  { 
    if (const llvm::json::Object* resObj = res.getAsObject())
    {
      std::optional<llvm::StringRef> kind = resObj->getString("kind");
      std::optional<llvm::StringRef> jsonFile = resObj->getString("jsonFile");

      if (!jsonFile.has_value()) {
        continue;
      }

      if (kind.has_value() && *kind == "codemodel") 
      {
        const bool success = parse_codemodel(*this, jsonFile->str());
        if (!success) {
          return false;
        }
      }
      else if (kind.has_value() && *kind == "toolchains") {
        const bool success = parse_toolchains(*this, jsonFile->str());
        if (!success) {
          return false;
        }
      }
    }
  }

  return true;
}

const CMakeToolchain* CMakeIndex::getToolchainByLanguage(const std::string& lang) const
{
  auto it = std::find_if(toolchains.begin(), toolchains.end(), [&lang](const CMakeToolchain& elem) {
    return elem.language == lang;
    });

  return it != toolchains.end() ? &(*it) : nullptr;
}

} // namespace cppscanner

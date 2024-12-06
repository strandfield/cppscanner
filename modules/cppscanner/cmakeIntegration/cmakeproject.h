// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_CMAKEPROJECT_H
#define CPPSCANNER_CMAKEPROJECT_H

#include <filesystem>
#include <map>
#include <memory>
#include <type_traits>
#include <vector>

namespace cppscanner
{

class CMakeProject
{
public:
  std::string name;
  std::vector<int> targetIndexes;
};

class CMakeTarget
{
public:
  std::string id;
  std::string name;
  int projectIndex = -1;

public:
  CMakeTarget(const std::string& id_, std::string name_) : 
    id(id_),
    name(std::move(name_))
  {

  }

  std::string type;

  std::vector<std::string> dependencies;

  std::vector<std::string> sources;

  struct CompileCommandFragment
  {
    std::string fragment;
  };

  struct CompileGroup
  {
    //enum Language
    //{
    //  LANG_C,
    //  LANG_CXX
    //};

    //enum LanguageStandard
    //{
    //  STD_CPP11,
    //  STD_CPP14,
    //  STD_CPP17,
    //  STD_CPP20,
    //  STD_CPP23,
    //  STD_CPP26,
    //};

    //Language language = LANG_C;
    //LanguageStandard languageStandard = STD_CPP17;
    std::string language;
    std::string languageStandard;
    std::vector<CompileCommandFragment> compileCommandFragments;
    std::vector<int> sourceIndexes;
  };

  std::vector<CompileGroup> compileGroups;
};


class CMakeConfiguration
{
public:
  std::string name;
  std::vector<CMakeProject> projects;
  std::vector<CMakeTarget> targets;

public:
  const CMakeTarget* getTargetByName(const std::string& name) const
  {
    auto it = std::find_if(targets.begin(), targets.end(), [&name](const CMakeTarget& elem) {
      return elem.name == name;
      });

    return it != targets.end() ? &(*it) : nullptr;
  }

  const CMakeTarget* getTargetById(const std::string& id) const
  {
    auto it = std::find_if(targets.begin(), targets.end(), [&id](const CMakeTarget& elem) {
      return elem.id == id;
      });

    return it != targets.end() ? &(*it) : nullptr;
  }
};

class CMakeToolchain
{
public:
  std::string language;

  struct Compiler
  {
    std::string id;
    std::filesystem::path path;
    std::string version;
  };

  Compiler compiler;
};

class CMakeIndex
{
public:
  std::filesystem::path indexJsonFile;

public:

  struct Paths
  {
    std::filesystem::path build;
    std::filesystem::path source;
  };

public:
  Paths paths;
  std::vector<CMakeConfiguration> configurations;
  std::vector<CMakeToolchain> toolchains;

public:

  bool read(const std::filesystem::path& path);

  const CMakeToolchain* getToolchainByLanguage(const std::string& lang) const;

};

} // namespace cppscanner

#endif // CPPSCANNER_CMAKEPROJECT_H

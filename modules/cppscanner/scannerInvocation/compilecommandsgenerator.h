// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_COMPILECOMMANDSGENERATOR_H
#define CPPSCANNER_COMPILECOMMANDSGENERATOR_H

#include "cppscanner/indexer/argumentsadjuster.h"
#include "cppscanner/indexer/scanner.h"
#include "cppscanner/cmakeIntegration/cmakeproject.h"

#include <filesystem>
#include <optional>

#include <cassert>
#include <iostream>

namespace cppscanner
{

inline void splitInto(const std::string& str, char sep, std::vector<std::string>& output)
{
  auto it = str.begin();

  while (it != str.end())
  {
    auto end = std::find(it, str.end(), sep);

    if (std::distance(it, end) > 0)
    {
      auto item = std::string(it, end);
      output.push_back(std::move(item));
    }

    it = end == str.end() ? end : std::next(end);
  }
}

inline const CMakeTarget::CompileGroup* getPrecompileHeaderGroup(const CMakeTarget& target)
{
  for (const CMakeTarget::CompileGroup& group : target.compileGroups)
  {
    if (group.precompileHeaders.empty()) {
      continue;
    }

    if (group.sourceIndexes.size() != 1) {
      continue;
    }

    const std::filesystem::path& src = target.sources.at(group.sourceIndexes.front());
    if (src.filename() == "cmake_pch.cxx")
    {
      return &group;
    }
  }

  return nullptr;
}

inline void generateCommandForPrecompileHeaderGroup(const CMakeIndex& index,
  const CMakeConfiguration& config, const CMakeTarget& target,const CMakeTarget::CompileGroup& group, std::vector<ScannerCompileCommand>& commands)
{
  std::vector<std::string> basecmd;

  const CMakeToolchain* toolchain = index.getToolchainByLanguage(group.language);

  if (!toolchain || toolchain->compiler.path.empty()) {
    return;
  }

  basecmd.push_back(toolchain->compiler.path.string());

  for (const CMakeTarget::CompileCommandFragment& fragment : group.compileCommandFragments)
  {
    splitInto(fragment.fragment, ' ', basecmd);
  }

  std::filesystem::path pch_input_file_path;
  {
    auto it = std::find_if(basecmd.begin(), basecmd.end(), [](const std::string& arg) {
      return arg.rfind("/Yc", 0) == 0;
      });

    if (it == basecmd.end()) {
      return;
    }

    pch_input_file_path = it->substr(3);
    //basecmd.erase(it);
  }

  std::filesystem::path pch_output_file_path;
  {
    auto it = std::find_if(basecmd.begin(), basecmd.end(), [](const std::string& arg) {
      return arg.rfind("/Fp", 0) == 0;
      });

    if (it == basecmd.end()) {
      return;
    }

    pch_output_file_path = it->substr(3);
    //basecmd.erase(it);
  }

  for (const std::string& define : group.defines)
  {
    basecmd.push_back("-D" + define);
  }

  for (const std::filesystem::path& includePath : group.includes)
  {
    basecmd.push_back("-I" + includePath.string());
  }

  assert(group.sourceIndexes.size() == 1);
  const int srcIndex = group.sourceIndexes.front();

  ScannerCompileCommand cmd;
  cmd.pch = pch_output_file_path;
  cmd.fileName = pch_input_file_path.generic_u8string();
  cmd.commandLine = basecmd;
  //cmd.commandLine.push_back("-Xclang");
  //cmd.commandLine.push_back("-emit-pch");
  //cmd.commandLine.push_back("-Xclang");
  //cmd.commandLine.push_back("-o");
  //cmd.commandLine.push_back("-Xclang");
  //cmd.commandLine.push_back(pch_output_file_path.generic_u8string());
  //cmd.commandLine.push_back(pch_input_file_path.generic_u8string());
  cmd.commandLine.push_back(cmd.fileName);

  commands.push_back(std::move(cmd));
}

inline void generateCommandsForTarget(const CMakeIndex& index,
  const CMakeConfiguration& config, const CMakeTarget& target, std::vector<ScannerCompileCommand>& commands)
{
  const clang::tooling::ArgumentsAdjuster& argsadjuster = getDefaultArgumentsAdjuster();

  const CMakeTarget::CompileGroup* pch_group = getPrecompileHeaderGroup(target);

#ifdef _WIN32
  if (pch_group) {
    generateCommandForPrecompileHeaderGroup(index, config, target, *pch_group, commands);
  }
#else
  pch_group = nullptr;
#endif

  for (const CMakeTarget::CompileGroup& group : target.compileGroups)
  {
    if (&group == pch_group) {
      continue;
    }

    std::vector<std::string> basecmd;

    const CMakeToolchain* toolchain = index.getToolchainByLanguage(group.language);

    if (!toolchain || toolchain->compiler.path.empty()) {
      continue;
    }

    basecmd.push_back(toolchain->compiler.path.string());

    for (const CMakeTarget::CompileCommandFragment& fragment : group.compileCommandFragments)
    {
      splitInto(fragment.fragment, ' ', basecmd);
    }

    for (const std::string& define : group.defines)
    {
      basecmd.push_back("-D" + define);
    }

    for (const std::filesystem::path& includePath : group.includes)
    {
      basecmd.push_back("-I" + includePath.string());
    }

    for (int srcIndex : group.sourceIndexes)
    {
      ScannerCompileCommand cmd;
      cmd.fileName = (index.paths.source / target.sources.at(srcIndex)).generic_u8string();
      cmd.commandLine = basecmd;
      cmd.commandLine = argsadjuster(cmd.commandLine, cmd.fileName);
      cmd.commandLine.push_back(cmd.fileName);
      commands.push_back(std::move(cmd));
    }
  }
}

inline std::vector<ScannerCompileCommand> generateCommandsForTargets(
  const CMakeIndex& index,
  const CMakeConfiguration& config,
  const std::vector<const CMakeTarget*>& targets)
{
  std::vector<ScannerCompileCommand> commands;


  for (const CMakeTarget* target : targets)
  {
    assert(target);

    generateCommandsForTarget(index, config, *target, commands);
  }

  return commands;
}


inline bool contains(const  std::vector<const CMakeTarget*>& targets, const CMakeTarget* item)
{
  return std::find(targets.begin(), targets.end(), item) != targets.end();
}

inline void addTargetDependenciesRecursive(
  const CMakeConfiguration& config,
  const std::vector<std::string>& targetIds, 
  std::vector<const CMakeTarget*>& targets)
{
  for (const std::string& targetId : targetIds)
  {
    const CMakeTarget* target = config.getTargetById(targetId);

    if (!target)
    {
      std::cerr << " no such target " << targetId << std::endl;
      continue;
    }

    if (contains(targets, target))
    {
      continue;
    }

    addTargetDependenciesRecursive(config, target->dependencies, targets);

    if (!contains(targets, target))
    {
      targets.push_back(target);
    }
  }
}

inline void addAllTargets(
  const CMakeConfiguration& config,
  std::vector<const CMakeTarget*>& targets)
{
  for (const CMakeTarget& target : config.targets)
  {
    if (contains(targets, &target))
    {
      continue;
    }

    addTargetDependenciesRecursive(config, target.dependencies, targets);

    if (!contains(targets, &target))
    {
      targets.push_back(&target);
    }
  }
}

inline std::vector<ScannerCompileCommand> generateCommands(
  const CMakeIndex& index,
  const CMakeConfiguration& config,
  const std::vector<std::string>& cmakeTargets)
{
  std::vector<const CMakeTarget*> targets;

  for (const std::string& targetName : cmakeTargets)
  {
    const CMakeTarget* target = config.getTargetByName(targetName);

    if (!target)
    {
      if (targetName == "all")
      {
        addAllTargets(config, targets);
      }
      else
      {
        std::cerr << " no such target " << targetName << std::endl;
      }

      continue;
    }

    if (contains(targets, target))
    {
      continue;
    }

    addTargetDependenciesRecursive(config, target->dependencies, targets);

    if (!contains(targets, target))
    {
      targets.push_back(target);
    }
  }

  return generateCommandsForTargets(index, config, targets);
}

inline std::vector<ScannerCompileCommand> generateCommands(
  const std::filesystem::path& cmakeBuildDirectory,
  const std::optional<std::string>& cmakeConfig,
  const std::vector<std::string>& cmakeTargets)
{
  CMakeIndex index;

  if (!index.read(cmakeBuildDirectory)) {
    return {};
  }

  auto it = index.configurations.begin();

  if (cmakeConfig.has_value())
  {
    const std::string& confname = cmakeConfig.value();
    it = std::find_if(index.configurations.begin(), index.configurations.end(), [&confname](const CMakeConfiguration& elem) {
      return elem.name == confname;
      });
  }

  if (it == index.configurations.end()) {
    return {};
  }

  return generateCommands(index, *it, cmakeTargets);
}

} // namespace cppscanner

#endif // CPPSCANNER_COMPILECOMMANDSGENERATOR_H

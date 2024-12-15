// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_ARGUMENTSADJUSTER_H
#define CPPSCANNER_ARGUMENTSADJUSTER_H

#include <clang/Tooling/ArgumentsAdjusters.h>

namespace cppscanner
{

inline clang::tooling::ArgumentsAdjuster getDefaultArgumentsAdjuster()
{
  clang::tooling::ArgumentsAdjuster syntaxOnly = clang::tooling::getClangSyntaxOnlyAdjuster();
  clang::tooling::ArgumentsAdjuster stripOutput = clang::tooling::getClangStripOutputAdjuster();
  clang::tooling::ArgumentsAdjuster detailedPreprocRecord = clang::tooling::getInsertArgumentAdjuster({ "-Xclang", "-detailed-preprocessing-record" }, clang::tooling::ArgumentInsertPosition::END);
  return clang::tooling::combineAdjusters(clang::tooling::combineAdjusters(syntaxOnly, stripOutput), detailedPreprocRecord);
}

inline const char* getDefaultCompilerExecutableName()
{
#ifdef _WIN32
  return "clang++";
#else
  return "/usr/bin/c++";
#endif
}

} // namespace cppscanner

#endif // CPPSCANNER_ARGUMENTSADJUSTER_H

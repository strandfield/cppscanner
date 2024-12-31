// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_ENV_H
#define CPPSCANNER_ENV_H

#include <optional>
#include <string>

namespace cppscanner
{

std::optional<std::string> readEnv(const char* varName);
bool isEnvTrue(const char* varName);

} // namespace cppscanner

#endif // CPPSCANNER_ENV_H

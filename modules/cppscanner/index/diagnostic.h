// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_DIAGNOSTIC_H
#define CPPSCANNER_DIAGNOSTIC_H

#include "fileid.h"
#include "fileposition.h"

#include <string>
#include <string_view>

namespace cppscanner
{

enum class DiagnosticLevel {
  Ignored = 0, 
  Note, 
  Remark, 
  Warning, 
  Error, 
  Fatal
};

inline constexpr std::string_view getDiagnosticLevelString(DiagnosticLevel lvl)
{
  switch (lvl)
  {
  case DiagnosticLevel::Ignored: return "ignored";
  case DiagnosticLevel::Note: return "note";
  case DiagnosticLevel::Remark: return "remark";
  case DiagnosticLevel::Warning: return "warning";
  case DiagnosticLevel::Error: return "error";
  case DiagnosticLevel::Fatal: return "fatal";
  default: return "<invalid>";
  }
}

template<typename F>
void enumerateDiagnosticLevel(F&& fn)
{
  fn(DiagnosticLevel::Ignored);
  fn(DiagnosticLevel::Note);
  fn(DiagnosticLevel::Remark);
  fn(DiagnosticLevel::Warning);
  fn(DiagnosticLevel::Error);
  fn(DiagnosticLevel::Fatal);
}

class Diagnostic
{
public:
  DiagnosticLevel level;
  std::string message;
  FileID fileID;
  FilePosition position;
};

} // namespace cppscanner

#endif // CPPSCANNER_DIAGNOSTIC_H

// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_TRANSLATIONUNITINDEX_H
#define CPPSCANNER_TRANSLATIONUNITINDEX_H

#include "cppscanner/index/baseof.h"
#include "cppscanner/index/diagnostic.h"
#include "cppscanner/index/include.h"
#include "cppscanner/index/override.h"
#include "cppscanner/index/reference.h"
#include "cppscanner/index/symbol.h"

#include <map>
#include <set>
#include <vector>

namespace cppscanner
{

class FileIdentificator;

/**
 * \brief the result of indexing a translation unit
 */
class TranslationUnitIndex
{
public:
  FileIdentificator* fileIdentificator = nullptr;

  std::vector<std::string> fileIdentifications; // remains empty unless 'fileIdentificator' differs from the one in scanner

  std::set<FileID> indexedFiles;

  std::vector<Include> ppIncludes;

  std::map<SymbolID, Symbol> symbols;

  std::vector<SymbolReference> symReferences;

  struct {
    std::vector<BaseOf> baseOfs;
    std::vector<Override> overrides;
  } relations;

  std::vector<Diagnostic> diagnostics;

public:
  void add(const Include& incl);
  void add(const SymbolReference& symRef);
  void add(const BaseOf& baseOf);
  void add(const Override& fnOverride);
  void add(Diagnostic d);
};


inline void TranslationUnitIndex::add(const Include& incl)
{
  ppIncludes.push_back(incl);
}

inline void TranslationUnitIndex::add(const SymbolReference& symRef)
{
  symReferences.push_back(symRef);
}

inline void TranslationUnitIndex::add(const BaseOf& baseOf)
{
  relations.baseOfs.push_back(baseOf);
}

inline void TranslationUnitIndex::add(const Override& fnOverride)
{
  relations.overrides.push_back(fnOverride);
}

inline void TranslationUnitIndex::add(Diagnostic d)
{
  diagnostics.push_back(std::move(d));
}

} // namespace cppscanner

#endif // CPPSCANNER_TRANSLATIONUNITINDEX_H

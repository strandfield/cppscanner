// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#include "translationunitindex.h"

#include <algorithm>

namespace cppscanner
{

int nbMissingFields(const SymbolReference& symref)
{
  return symref.referencedBySymbolID.isValid() ? 0 : 1;
}

void sortAndRemoveDuplicates(std::vector<SymbolReference>& refs)
{
  std::sort(
    refs.begin(), 
    refs.end(), 
    [](const SymbolReference& a, const SymbolReference& b) -> bool {
      return std::forward_as_tuple(a.fileID, a.position, a.symbolID, nbMissingFields(a)) < 
        std::forward_as_tuple(b.fileID, b.position, b.symbolID, nbMissingFields(b));
    }
  );

  // remove duplicates
  {
    auto equiv = [](const SymbolReference& a, const SymbolReference& b) -> bool {
      return std::forward_as_tuple(a.fileID, a.position, a.symbolID) == std::forward_as_tuple(b.fileID, b.position, b.symbolID);
      };

    auto it = std::unique(refs.begin(), refs.end(), equiv);
    refs.erase(it, refs.end());
  }
}

void sortAndRemoveDuplicates(std::vector<ArgumentPassedByReference>& refargs)
{
  std::sort(refargs.begin(), refargs.end());

  // remove duplicates
  {
    auto it = std::unique(refargs.begin(), refargs.end());
    refargs.erase(it, refargs.end());
  }
}

} // namespace cppscanner

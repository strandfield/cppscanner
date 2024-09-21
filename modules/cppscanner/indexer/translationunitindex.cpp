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
  // TODO: sorting and removing duplicates could be done in the parsing thread.
  // handle that with a flag indicating whether the vector is sorted.
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

} // namespace cppscanner

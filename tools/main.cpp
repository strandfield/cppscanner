
#include "cppscanner/index/symbolkind.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[])
{
  size_t count = 0;
  std::vector<cppscanner::SymbolKind> names;

  cppscanner::enumerateSymbolKind([&names, &count](cppscanner::SymbolKind k) {
    auto offset = static_cast<size_t>(k);
    if (offset >= names.size()) {
      names.resize(offset + 1, cppscanner::SymbolKind::Unknown);
    }
    names[offset] = k;
    ++count;
  });

  std::ofstream stream{ "symbolkind.js", std::ios::out | std::ios::trunc };

  stream << "export const symbolKinds = {" << std::endl;
  stream << "  names: [" << std::endl;
  for (size_t i(0); i < names.size(); ++i) {
    stream << "    \"" << cppscanner::getSymbolKindString(names.at(i)) << "\"";
    if (i != names.size() - 1) {
      stream << ",";
    }
    stream << std::endl;
  }
  stream << "  ]," << std::endl;
  stream << "  values: {" << std::endl;
  cppscanner::enumerateSymbolKind([&stream, &names, &count](cppscanner::SymbolKind k) {
    stream << "    \"" << cppscanner::getSymbolKindString(k) << "\": " << static_cast<int>(k);
    if (--count > 0) {
      stream << ",";
    }
    stream << std::endl;
    });
  stream << "  }" << std::endl;
  stream << "};" << std::endl;
}

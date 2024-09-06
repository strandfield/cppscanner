
#pragma once

#include "cppscanner/index/file.h"
#include "cppscanner/index/symbol.h"
#include "cppscanner/index/reference.h"

#include "cppscanner/indexer/snapshot.h"

#include <vector>
#include <optional>
#include <regex>

inline cppscanner::File getFile(const std::vector<cppscanner::File>& files, const std::regex& pattern)
{
  for (const cppscanner::File& f : files) {
    if (std::regex_search(f.path, pattern)) {
      return f;
    }
  }

  throw std::runtime_error("no match found");
}

class SymbolRefPattern
{
public:
  std::optional<cppscanner::FileID> fileID;
  std::optional<cppscanner::SymbolID> symbolID;
  std::optional<int> line;

public:
  explicit SymbolRefPattern(const cppscanner::SymbolRecord& s) : symbolID(s.id) { }

  SymbolRefPattern& inFile(const cppscanner::File& f) {
    this->fileID = f.id;
    return *this;
  }

  SymbolRefPattern& at(int line) {
    this->line = line;
    return *this;
  }
};

inline bool match(const cppscanner::SymbolReference& ref, const SymbolRefPattern& pattern)
{
  bool file = !pattern.fileID.has_value() || *pattern.fileID == ref.fileID;
  bool line = !pattern.line.has_value() || *pattern.line == ref.position.line();
  bool symbol = !pattern.symbolID.has_value() || *pattern.symbolID == ref.symbolID;
  return file && line && symbol;
}

inline bool containsRef(const std::vector<cppscanner::SymbolReference>& refs, const SymbolRefPattern& pattern)
{
  auto search = [&pattern](const cppscanner::SymbolReference& r) -> bool {
    return match(r, pattern);
    };

  auto it = std::find_if(refs.begin(), refs.end(), search);
  return it != refs.end();
}

inline void filterRefs(std::vector<cppscanner::SymbolReference>& refs, const SymbolRefPattern& pattern)
{
  auto pred = [&pattern](const cppscanner::SymbolReference& r) -> bool {
    return !match(r, pattern);
    };

  auto it = std::remove_if(refs.begin(), refs.end(), pred);
  refs.erase(it, refs.end());
}

class TemporarySnapshot : public cppscanner::Snapshot
{
private:
  std::filesystem::path m_db_path;
public:
  bool delete_on_close = true;

public:
  explicit TemporarySnapshot(const std::filesystem::path& p)
    : Snapshot(Snapshot::open(p)),
    m_db_path(p)
  {

  }

  ~TemporarySnapshot()
  {
    database().close();
    if (delete_on_close) {
      std::filesystem::remove(m_db_path);
    }
  }
};
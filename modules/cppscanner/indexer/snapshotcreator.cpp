// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#include "snapshotcreator.h"

#include "fileidentificator.h"
#include "fileindexingarbiter.h"
#include "translationunitindex.h"
#include "vector-of-bool.h"

#include "cppscanner/database/transaction.h"

#include "cppscanner/base/glob.h"
#include "cppscanner/base/os.h"
#include "cppscanner/base/version.h"

#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/StringExtras.h> // toHex
#include <llvm/Support/SHA1.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <optional>
#include <vector>

namespace cppscanner
{

struct SnapshotCreatorData
{
  std::string homeDirectory;
  bool captureFileContent = true;

  std::vector<bool> filePathsInserted;
  std::vector<bool> indexedFiles;
  std::map<SymbolID, IndexerSymbol> symbols;
};


SnapshotCreator::~SnapshotCreator()
{
  if (m_snapshot)
  {
    close();
  }
}

SnapshotCreator::SnapshotCreator(FileIdentificator& fileIdentificator)
  : m_fileIdentificator(fileIdentificator)
{
  d = std::make_unique<SnapshotCreatorData>();
}

FileIdentificator& SnapshotCreator::fileIdentificator() const
{
  return m_fileIdentificator;
}

void SnapshotCreator::setHomeDir(const std::filesystem::path& p)
{
  d->homeDirectory = p.generic_u8string();
  writeHomeProperty();
}

void SnapshotCreator::setCaptureFileContent(bool on)
{
  d->captureFileContent = on;
}


/**
 * \brief creates an empty snapshot
 * \param dbPath  the path of the database
 */
void SnapshotCreator::init(const std::filesystem::path& dbPath)
{
  m_snapshot = std::make_unique<SnapshotWriter>(dbPath);

  m_snapshot->setProperty("cppscanner.version", cppscanner::versioncstr());
  m_snapshot->setProperty("cppscanner.os", cppscanner::system_name());
  writeHomeProperty();
}

SnapshotWriter* SnapshotCreator::snapshotWriter() const
{
  return m_snapshot.get();
}

void SnapshotCreator::writeProperty(const std::string& name, const std::string& value)
{
  m_snapshot->setProperty(name, value);
}

namespace
{

std::set<FileID> listIncludedFiles(const std::vector<Include>& includes)
{
  std::set<FileID> ids;
  
  for (const Include& incl : includes) {
    ids.insert(incl.includedFileID);
  }

  return ids;
}

std::vector<Include> merge(
  std::vector<Include>&& existingIncludes, 
  std::vector<Include>::const_iterator newIncludesBegin, 
  std::vector<Include>::const_iterator newIncludesEnd)
{
  existingIncludes.insert(existingIncludes.end(), newIncludesBegin, newIncludesEnd);

  auto comp = [](const Include& a, const Include& b) -> bool {
    return std::forward_as_tuple(a.fileID, a.line) < std::forward_as_tuple(b.fileID, b.line);
    };

  std::sort(existingIncludes.begin(),  existingIncludes.end(), comp);

  auto eq = [](const Include& a, const Include& b) -> bool {
    return std::forward_as_tuple(a.fileID, a.line) == std::forward_as_tuple(b.fileID, b.line);
    };

  auto new_logical_end = std::unique(existingIncludes.begin(), existingIncludes.end(), eq);
  existingIncludes.erase(new_logical_end, existingIncludes.end());

  return existingIncludes;
}

std::vector<SymbolReference>& insertOrIgnore(
  std::vector<SymbolReference>& references, 
  std::vector<SymbolReference>::const_iterator newReferencesBegin, 
  std::vector<SymbolReference>::const_iterator newReferencesEnd)
{
  references.insert(references.end(), newReferencesBegin, newReferencesEnd);

  auto ref_comp = [](const SymbolReference& a, const SymbolReference& b) -> bool {
    return std::forward_as_tuple(a.fileID, a.position, a.symbolID) < std::forward_as_tuple(b.fileID, b.position, b.symbolID);
    };

  std::sort(references.begin(),  references.end(), ref_comp);

  auto ref_eq = [](const SymbolReference& a, const SymbolReference& b) -> bool {
    return std::forward_as_tuple(a.fileID, a.position, a.symbolID) == std::forward_as_tuple(b.fileID, b.position, b.symbolID);
    };

  auto new_logical_end = std::unique(references.begin(), references.end(), ref_eq);
  references.erase(new_logical_end, references.end());

  return references;
}

inline FileID fileOf(const Diagnostic& d)
{
  return d.fileID;
}

inline FileID fileOf(const SymbolDeclaration& d)
{
  return d.fileID;
}

template<typename T>
void sortByFile(std::vector<T>& elements)
{
  std::sort(
    elements.begin(), 
    elements.end(), 
    [](const T& a, const T& b) -> bool {
      return fileOf(a) < fileOf(b);
    }
  );
}

template<typename T>
typename std::vector<T>::iterator fileRangeEnd(std::vector<T>& elements, typename std::vector<T>::iterator begin)
{
  FileID cur_file_id = fileOf(*begin);
  return std::find_if(std::next(begin), elements.end(),
    [cur_file_id](const T& element) {
      return fileOf(element) != cur_file_id;
    }
  );
}

bool mergeComp(const Diagnostic& a, const Diagnostic& b)
{
  return std::forward_as_tuple(a.level, a.position, a.message) <
    std::forward_as_tuple(b.level, b.position, b.message);
}

bool mergeComp(const SymbolDeclaration& a, const SymbolDeclaration& b)
{
  // we ignore file id because it is assumed they are the same
  assert(a.fileID == b.fileID);
  return std::forward_as_tuple(a.symbolID, a.startPosition, a.endPosition, a.isDefinition) <
    std::forward_as_tuple(b.symbolID, b.startPosition, b.endPosition, b.isDefinition);
}

template<typename T>
bool mergeEq(const T& a, const T& b)
{
  return !mergeComp(a, b) && !mergeComp(b, a);
}

template<typename T>
std::vector<T> merge(
  std::vector<T>&& existingElements, 
  typename std::vector<T>::const_iterator newElementsBegin, 
  typename std::vector<T>::const_iterator newElementsEnd)
{
  existingElements.insert(existingElements.end(), newElementsBegin, newElementsEnd);
  std::sort(existingElements.begin(), existingElements.end(), [](const T& a, const T& b) { return mergeComp(a, b); });
  auto new_logical_end = std::unique(existingElements.begin(), existingElements.end(), [](const T& a, const T& b) { return mergeEq(a, b); });
  existingElements.erase(new_logical_end, existingElements.end());
  return existingElements;
}

void removeCarriageReturns(std::string& text)
{
#ifndef _WIN32
  return;
#endif // !_WIN32

  auto it = std::remove_if(text.begin(), text.end(), [](char c) {
    return c == '\r';
    });

  text.erase(it, text.end());
}

std::string computeSha1(const std::string& text)
{
  llvm::SHA1 hasher;
  hasher.update(text);
  std::array<uint8_t, 20> result = hasher.final();
  constexpr bool to_lower_case = true;
  return llvm::toHex(result, to_lower_case);
}

} // namespace

void SnapshotCreator::fillContent(File& f)
{
  {
    std::ifstream stream{ f.path };
    if (stream.good()) 
    {
      stream.seekg(0, std::ios::end);
      f.content.reserve(stream.tellg());
      stream.seekg(0, std::ios::beg);
      f.content.assign((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    }
  }

  if (!f.content.empty())
  {
    removeCarriageReturns(f.content);
    f.sha1 = computeSha1(f.content);
  }
}

void SnapshotCreator::feed(const TranslationUnitIndex& tuIndex)
{
  TranslationUnitIndex copy = tuIndex;
  feed(std::move(copy));
}

void SnapshotCreator::feed(TranslationUnitIndex&& tuIndex)
{
  const std::vector<std::string> known_files = fileIdentificator().getFiles();

  std::vector<File> newfiles;

  {
    for (FileID fid : tuIndex.indexedFiles)
    {
      if (fileAlreadyIndexed(fid)) {
        continue;
      }

      File f;
      f.id = fid;
      f.path = known_files.at(fid);

      if (d->captureFileContent)
      {
        fillContent(f);
      }

      newfiles.push_back(std::move(f));
    }

    {
      sql::TransactionScope transaction{ m_snapshot->database() };
      m_snapshot->insertFiles(newfiles);
    }

    for (const File& f : newfiles) {
      set_flag(d->filePathsInserted, f.id);
    }
  }

  {
    // ensure that included files are listed in the database
    {
      std::set<FileID> included = listIncludedFiles(tuIndex.ppIncludes);
      std::vector<File> newincludes;

      for (FileID fid : included)
      {
        if (test_flag(d->filePathsInserted, fid)) {
          continue;
        }

        File f;
        f.id = fid;
        f.path = known_files.at(fid);
        newincludes.push_back(std::move(f));
      }

      sql::runTransacted(m_snapshot->database(), [this, &newincludes]() {
        m_snapshot->insertFilePaths(newincludes);
        });

      for (const File& f : newincludes) {
        set_flag(d->filePathsInserted, f.id);
      }
    }
    
    std::sort(tuIndex.ppIncludes.begin(), tuIndex.ppIncludes.end(), [](const Include& a, const Include& b) {
      return a.fileID < b.fileID;
      });

    auto it = tuIndex.ppIncludes.begin();

    while (it != tuIndex.ppIncludes.end()) {
      FileID cur_file_id = it->fileID;
      auto end = std::find_if(std::next(it), tuIndex.ppIncludes.end(),
        [cur_file_id](const Include& element) {
          return element.fileID != cur_file_id;
        }
      );

      if (fileAlreadyIndexed(cur_file_id)) {
        std::vector<Include> includes = merge(m_snapshot->loadAllIncludesInFile(cur_file_id), it, end);

        sql::runTransacted(m_snapshot->database(), [this, cur_file_id, &includes]() {
          m_snapshot->removeAllIncludesInFile(cur_file_id);
          m_snapshot->insertIncludes(includes);
          });
      } else {
        sql::runTransacted(m_snapshot->database(), [this, it, end]() {
          m_snapshot->insertIncludes(std::vector<Include>(it, end));
          });
      }

      it = end;
    }
  }

  // insert new symbols, update the others that need it
  {
    std::vector<const IndexerSymbol*> symbols_to_insert;
    std::vector<const IndexerSymbol*> symbols_with_flags_to_update;

    for (auto& p : tuIndex.symbols) {
      auto it = d->symbols.find(p.first);
      if (it == d->symbols.end()) {
        IndexerSymbol& newsymbol = d->symbols[p.first];
        newsymbol = std::move(p.second);
        symbols_to_insert.push_back(&newsymbol);
      } else {
        IndexerSymbol& existing_symbol = it->second;
        int what_updated = update(existing_symbol, p.second);
        if (what_updated & IndexerSymbol::FlagUpdate)
        {
          symbols_with_flags_to_update.push_back(&existing_symbol);
        }
      }
    }

    sql::runTransacted(m_snapshot->database(), [this, &symbols_to_insert, &symbols_with_flags_to_update]() {
      m_snapshot->insertSymbols(symbols_to_insert);
      m_snapshot->updateSymbolsFlags(symbols_with_flags_to_update);
      });
  }

  // Process symbol references
  {
    auto it = tuIndex.symReferences.begin();

    while (it != tuIndex.symReferences.end()) {
      FileID cur_file_id = it->fileID;

      auto end = std::find_if(std::next(it), tuIndex.symReferences.end(),
        [cur_file_id](const SymbolReference& ref) {
          return ref.fileID != cur_file_id;
        }
      );

      if (fileAlreadyIndexed(cur_file_id)) 
      {
        std::vector<SymbolReference> references = m_snapshot->loadSymbolReferencesInFile(cur_file_id);
        insertOrIgnore(references, it, end);

        {
          sql::TransactionScope transaction{ m_snapshot->database() };
          m_snapshot->removeAllSymbolReferencesInFile(cur_file_id);
          m_snapshot->insert(references);
        }
      } 
      else 
      {
        sql::TransactionScope transaction{ m_snapshot->database() };
        m_snapshot->insert(std::vector<SymbolReference>(it, end));
      }

      it = end;
    }
  }

  // Process relations
  {
    m_snapshot->insertBaseOfs(tuIndex.relations.baseOfs);
    m_snapshot->insertOverrides(tuIndex.relations.overrides);
  }

  // Process diagnostics
  {
    sortByFile(tuIndex.diagnostics);

    for (auto it = tuIndex.diagnostics.begin(); it != tuIndex.diagnostics.end(); ) {
      FileID cur_file_id = it->fileID;
      auto end = fileRangeEnd(tuIndex.diagnostics, it);

      if (fileAlreadyIndexed(cur_file_id)) {
        std::vector<Diagnostic> diagnostics = merge(m_snapshot->loadDiagnosticsInFile(cur_file_id), it, end);

        sql::runTransacted(m_snapshot->database(), [this, cur_file_id, &diagnostics]() {
          m_snapshot->removeAllDiagnosticsInFile(cur_file_id);
          m_snapshot->insertDiagnostics(diagnostics);
          });
      } else {
        sql::runTransacted(m_snapshot->database(), [this, it, end]() {
          m_snapshot->insertDiagnostics(std::vector<Diagnostic>(it, end));
          });
      }

      it = end;
    }
  }

  // Process refargs
  {
    sql::TransactionScope transaction{ m_snapshot->database() };
    m_snapshot->insert(tuIndex.fileAnnotations.refargs);
  }

  // Process symbol declarations
  {
    // declarations are sorted by file.
#ifndef  NDEBUG
    bool sorted = std::is_sorted(tuIndex.declarations.begin(), tuIndex.declarations.end(), 
      [](const SymbolDeclaration& a, const SymbolDeclaration& b) {
        return a.fileID < b.fileID; 
      });
    assert(sorted);
#endif // !NDEBUG

    for (auto it = tuIndex.declarations.begin(); it != tuIndex.declarations.end(); ) {
      FileID cur_file_id = it->fileID;
      auto end = fileRangeEnd(tuIndex.declarations, it);

      if (fileAlreadyIndexed(cur_file_id)) {
        std::vector<SymbolDeclaration> declarations = merge(m_snapshot->loadDeclarationsInFile(cur_file_id), it, end);

        sql::TransactionScope transaction{ m_snapshot->database() };
        m_snapshot->removeAllDeclarationsInFile(cur_file_id);
        m_snapshot->insert(declarations);
      } else {
        sql::TransactionScope transaction{ m_snapshot->database() };
        m_snapshot->insert(std::vector<SymbolDeclaration>(it, end));
      }

      it = end;
    }    
  }

  // Update list of already indexed files
  for (const File& f : newfiles) {
    setFileIndexed(f.id);
  }
}

void SnapshotCreator::close()
{
  if (m_snapshot)
  {
    m_snapshot.reset();
  }
}

void SnapshotCreator::writeHomeProperty()
{
  if (m_snapshot)
  {
    m_snapshot->setProperty("project.home", Snapshot::Path(d->homeDirectory));
  }
}

bool SnapshotCreator::fileAlreadyIndexed(FileID f) const
{
  return test_flag(d->indexedFiles, static_cast<size_t>(f));
}

void SnapshotCreator::setFileIndexed(FileID f)
{
  set_flag(d->indexedFiles, static_cast<size_t>(f));
}

} // namespace cppscanner

// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#include "scanner.h"

#include "version.h"

#include "indexer.h"
#include "indexingresultqueue.h"

#include "frontendactionfactory.h"
#include "fileindexingarbiter.h"
#include "translationunitindex.h"

#include "basicfileidentificator.h"

#include "cppscanner/database/transaction.h"

#include <clang/Tooling/JSONCompilationDatabase.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <optional>

namespace cppscanner
{

struct ScannerData
{
  std::string homeDirectory;
  std::optional<std::string> rootDirectory;
  bool indexExternalFiles = false;
  bool indexLocalSymbols = false;
  std::vector<std::string> filters;
  std::unique_ptr<FileIdentificator> fileIdentificator;
  std::vector<bool> filePathsInserted;
  std::vector<bool> indexedFiles;
  std::map<SymbolID, Symbol> symbols;
};

void set_flag(std::vector<bool>& flags, size_t i)
{
  if (flags.size() <= i) {
    flags.resize(i + 1, false);
  }

  flags[i] = true;
}

bool test_flag(const std::vector<bool>& flags, size_t i)
{
  return i < flags.size() && flags.at(i);
}

std::unique_ptr<FileIndexingArbiter> createIndexingArbiter(ScannerData& d)
{
  std::vector<std::unique_ptr<FileIndexingArbiter>> arbiters;

  arbiters.push_back(std::make_unique<IndexOnceFileIndexingArbiter>(*d.fileIdentificator));

  if (d.indexExternalFiles) {
    if (d.rootDirectory.has_value()) {
      arbiters.push_back(std::make_unique<IndexDirectoryFileIndexingArbiter>(*d.fileIdentificator, *d.rootDirectory));
    }
  } else {
    arbiters.push_back(std::make_unique<IndexDirectoryFileIndexingArbiter>(*d.fileIdentificator, d.homeDirectory));
  }

  if (!d.filters.empty()) {
    arbiters.push_back(std::make_unique<IndexFilesMatchingPatternIndexingArbiter>(*d.fileIdentificator, d.filters));
  }

  return FileIndexingArbiter::createCompositeArbiter(std::move(arbiters));
}

Scanner::Scanner()
{
  d = std::make_unique<ScannerData>();
  d->homeDirectory = std::filesystem::current_path().generic_u8string();
  d->fileIdentificator = std::make_unique<BasicFileIdentificator>();
}

Scanner::~Scanner() = default;

void Scanner::setHomeDir(const std::filesystem::path& p)
{
  d->homeDirectory = p.generic_u8string();
}

void Scanner::setRootDir(const std::filesystem::path& p)
{
  d->rootDirectory = p.generic_u8string();
}

void Scanner::setIndexExternalFiles(bool on)
{
  d->indexExternalFiles = on;
}

void Scanner::setIndexLocalSymbols(bool on)
{
  d->indexLocalSymbols = on;
}

void Scanner::setFilters(const std::vector<std::string>& filters)
{
  d->filters = filters;
}

/**
 * \brief creates an empty snapshot
 * \param p  the path of the database
 */
void Scanner::initSnapshot(const std::filesystem::path& p)
{
  m_snapshot = std::make_unique<Snapshot>(Snapshot::create(p));

  m_snapshot->setProperty("cppscanner.version", cppscanner::versioncstr());

#ifdef _WIN32
  m_snapshot->setProperty("cppscanner.os", "windows");
#else
  m_snapshot->setProperty("cppscanner.os", "linux");
#endif // _WIN32

  m_snapshot->setProperty("project.home", Snapshot::Path(d->homeDirectory));

  m_snapshot->setProperty("scanner.indexExternalFiles", d->indexExternalFiles);
  m_snapshot->setProperty("scanner.indexLocalSymbols", d->indexLocalSymbols);
  m_snapshot->setProperty("scanner.root", Snapshot::Path(d->rootDirectory.value_or(std::string())));
}

Snapshot* Scanner::snapshot() const
{
  return m_snapshot.get();
}

void Scanner::scan(const std::filesystem::path& compileCommandsPath)
{
  std::string error_message;
  std::unique_ptr<clang::tooling::JSONCompilationDatabase> compile_commands = clang::tooling::JSONCompilationDatabase::loadFromFile(
      compileCommandsPath.u8string().c_str(), std::ref(error_message), clang::tooling::JSONCommandLineSyntax::AutoDetect
  );

  Snapshot& snapshot = *m_snapshot;
  std::unique_ptr<FileIndexingArbiter> indexing_arbiter = createIndexingArbiter(*d);
  clang::IntrusiveRefCntPtr<clang::FileManager> file_manager{ new clang::FileManager(clang::FileSystemOptions()) };
  IndexingResultQueue results_queue;
  auto index_data_consumer = std::make_shared<Indexer>(*indexing_arbiter, results_queue);
  IndexingFrontendActionFactory actionfactory{ index_data_consumer };
  actionfactory.setIndexLocalSymbols(d->indexLocalSymbols);

  clang::tooling::ArgumentsAdjuster syntaxOnly = clang::tooling::getClangSyntaxOnlyAdjuster();
  clang::tooling::ArgumentsAdjuster stripOutput = clang::tooling::getClangStripOutputAdjuster();
  clang::tooling::ArgumentsAdjuster detailedPreprocRecord = clang::tooling::getInsertArgumentAdjuster({ "-Xclang", "-detailed-preprocessing-record" }, clang::tooling::ArgumentInsertPosition::END);
  clang::tooling::ArgumentsAdjuster argsAdjuster = clang::tooling::combineAdjusters(clang::tooling::combineAdjusters(syntaxOnly, stripOutput), detailedPreprocRecord);

  for (const clang::tooling::CompileCommand& cc : compile_commands->getAllCompileCommands())
  {
     std::vector<std::string> command = argsAdjuster(cc.CommandLine, cc.Filename);
    /* std::vector<std::string> command = clang::tooling::getClangSyntaxOnlyAdjuster()(cc.CommandLine, cc.Filename);
    command = clang::tooling::getClangStripOutputAdjuster()(command, cc.Filename);*/
    clang::tooling::ToolInvocation invocation{ std::move(command), actionfactory.create(), file_manager.get() };

    invocation.setDiagnosticConsumer(index_data_consumer->getDiagnosticConsumer());

    std::cout << cc.Filename;

    if (invocation.run()) {
      assimilate(results_queue.read());
      std::cout << " [OK]" << std::endl;
    } else {
      std::cout << " [ERROR]" << std::endl;
    }
  }
}

namespace
{

std::vector<FileID> createRemapTable(const std::vector<std::string>& filePaths, FileIdentificator& fIdentificator)
{
  std::vector<FileID> result;
  result.reserve(filePaths.size());

  for (const std::string& file : filePaths) {
    result.push_back(fIdentificator.getIdentification(file));
  }

  return result;
}

void remapFileIds(TranslationUnitIndex& tuIndex, FileIdentificator& targetFileIdentificator)
{
  std::vector<FileID> newids = createRemapTable(
    tuIndex.fileIdentifications,
    targetFileIdentificator
  );

  tuIndex.fileIdentifications.clear();

  {
    decltype(tuIndex.indexedFiles) indexed_files;

    for (FileID fid : tuIndex.indexedFiles) {
      indexed_files.insert(newids[fid]);
    }

    std::swap(tuIndex.indexedFiles, indexed_files);
  }

  for (Include& inc : tuIndex.ppIncludes) {
    inc.fileID = newids[inc.fileID];
    inc.includedFileID = newids[inc.includedFileID];
  }

  for (SymbolReference& ref : tuIndex.symReferences) {
    ref.fileID = newids[ref.fileID];
  }
}

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
  auto new_logical_end = std::unique(existingIncludes.begin(), existingIncludes.end(), comp);
  existingIncludes.erase(new_logical_end, existingIncludes.end());
  return existingIncludes;
}

std::vector<SymbolReference> merge(
  std::vector<SymbolReference>&& existingReferences, 
  std::vector<SymbolReference>::const_iterator newReferencesBegin, 
  std::vector<SymbolReference>::const_iterator newReferencesEnd)
{
  existingReferences.insert(existingReferences.end(), newReferencesBegin, newReferencesEnd);

  auto ref_comp = [](const SymbolReference& a, const SymbolReference& b) -> bool {
    return std::forward_as_tuple(a.fileID, a.position, a.symbolID) < std::forward_as_tuple(b.fileID, b.position, b.symbolID);
    };

  std::sort(existingReferences.begin(),  existingReferences.end(), ref_comp);
  auto new_logical_end = std::unique(existingReferences.begin(), existingReferences.end(), ref_comp);
  existingReferences.erase(new_logical_end, existingReferences.end());
  return existingReferences;
}

inline FileID fileOf(const Diagnostic& d)
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

template<typename T>
std::vector<T> merge(
  std::vector<T>&& existingElements, 
  typename std::vector<T>::const_iterator newElementsBegin, 
  typename std::vector<T>::const_iterator newElementsEnd)
{
  existingElements.insert(existingElements.end(), newElementsBegin, newElementsEnd);
  std::sort(existingElements.begin(),  existingElements.end(), mergeComp);
  auto new_logical_end = std::unique(existingElements.begin(), existingElements.end(), mergeComp);
  existingElements.erase(new_logical_end, existingElements.end());
  return existingElements;
}

int nbMissingFields(const SymbolReference& symref)
{
  return symref.referencedBySymbolID.isValid() ? 0 : 1;
}

} // namespace

void Scanner::assimilate(TranslationUnitIndex tuIndex)
{
  if (tuIndex.fileIdentificator != d->fileIdentificator.get()) {
    remapFileIds(tuIndex, *d->fileIdentificator);
  }

  const std::vector<std::string>& known_files = d->fileIdentificator->getFiles();

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

      std::ifstream file{ f.path };
      if (file.good()) {
        file.seekg(0, std::ios::end);
        f.content.reserve(file.tellg());
        file.seekg(0, std::ios::beg);
        f.content.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
      }

      newfiles.push_back(std::move(f));
    }

    sql::runTransacted(m_snapshot->database(), [this, &newfiles]() {
      snapshot::insertFiles(*m_snapshot, newfiles);
      });

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
          m_snapshot->removeAllSymbolReferencesInFile(cur_file_id);
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

  // TODO: insert new symbols, update the others that need it
  {
    std::vector<const Symbol*> symbols;

    for (auto& p : tuIndex.symbols) {
      if (d->symbols.find(p.first) == d->symbols.end()) {
        Symbol& newsymbol = d->symbols[p.first];
        newsymbol = std::move(p.second);
        symbols.push_back(&newsymbol);
      } else {
        // TODO: check if the symbol needs to be updated
      }
    }

    sql::runTransacted(m_snapshot->database(), [this, &symbols]() {
      m_snapshot->insertSymbols(symbols);
      });
  }

  // Process symbol references
  {
    // TODO: sorting and removing duplicates could be done in the parsing thread.
    // handle that with a flag indicating whether the vector is sorted.
    std::sort(
      tuIndex.symReferences.begin(), 
      tuIndex.symReferences.end(), 
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

      auto it = std::unique(tuIndex.symReferences.begin(), tuIndex.symReferences.end(), equiv);
      tuIndex.symReferences.erase(it, tuIndex.symReferences.end());
    }

    auto it = tuIndex.symReferences.begin();

    while (it != tuIndex.symReferences.end()) {
      FileID cur_file_id = it->fileID;

      auto end = std::find_if(std::next(it), tuIndex.symReferences.end(),
        [cur_file_id](const SymbolReference& ref) {
          return ref.fileID != cur_file_id;
        }
      );

      if (fileAlreadyIndexed(cur_file_id)) {
        std::vector<SymbolReference> references = merge(m_snapshot->loadSymbolReferencesInFile(cur_file_id), it, end);

        sql::runTransacted(m_snapshot->database(), [this, cur_file_id, &references]() {
          m_snapshot->removeAllSymbolReferencesInFile(cur_file_id);
          snapshot::insertSymbolReferences(*m_snapshot, references);
          });
      } else {
        sql::runTransacted(m_snapshot->database(), [this, it, end]() {
          snapshot::insertSymbolReferences(*m_snapshot, std::vector<SymbolReference>(it, end));
          });
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

  // Update list of already indexed files
  for (const File& f : newfiles) {
    setFileIndexed(f.id);
  }
}

bool Scanner::fileAlreadyIndexed(FileID f) const
{
  return d->indexedFiles.size() > f && d->indexedFiles.at(f);
}

void Scanner::setFileIndexed(FileID f)
{
  if (d->indexedFiles.size() <= f) {
    d->indexedFiles.resize(f + 1, false);
  }

  d->indexedFiles[f] = true;
}

} // namespace cppscanner

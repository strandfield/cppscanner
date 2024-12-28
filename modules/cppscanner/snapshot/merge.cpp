// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#include "merge.h"

#include "cppscanner/snapshot/indexersymbol.h"
#include "cppscanner/snapshot/symbolrecorditerator.h"

#include "cppscanner/base/os.h"
#include "cppscanner/base/version.h"

#include <algorithm>
#include <cassert>
#include <set>

namespace cppscanner
{

namespace
{

std::vector<File>::const_iterator partitionByProjectStatus(std::vector<File>& files, const std::string& homePath)
{
  const std::string prefix = homePath + "/";

  return std::partition(files.begin(), files.end(), [&prefix](const File& f) {
    return f.path.rfind(prefix, 0) == 0;
    });
}

std::vector<FileID> createRemapTable(const std::vector<File>& files, const FileIdTable& table)
{
  std::vector<FileID> result;

  for (const File& file : files) 
  {
    if (file.id >= result.size())
    {
      result.resize(file.id+1);
    }

    result[file.id] = table.findIdentification(file.path);
  }

  return result;
}

void sortAndRemoveDuplicates(std::vector<Include>& list)
{
  std::sort(list.begin(), list.end(), [](const Include& a, const Include& b) {
    return std::forward_as_tuple(a.fileID, a.line, a.includedFileID) < std::forward_as_tuple(b.fileID, b.line, b.includedFileID);
    });
  
  auto it = std::unique(list.begin(), list.end(), [](const Include& a, const Include& b) {
    return std::forward_as_tuple(a.fileID, a.line, a.includedFileID) == std::forward_as_tuple(b.fileID, b.line, b.includedFileID);
    });

  list.erase(it, list.end());
}

auto get_as_tuple(const ArgumentPassedByReference& value)
{
  return std::forward_as_tuple(value.fileID, value.position);
}

auto get_as_tuple(const Diagnostic& value)
{
  return std::forward_as_tuple(value.fileID, value.level, value.position, value.message);
}

auto get_as_tuple(const SymbolReference& value)
{
  return std::forward_as_tuple(value.fileID, value.position, value.symbolID, value.referencedBySymbolID, value.flags);
}

auto get_as_tuple(const SymbolDeclaration& value)
{
  return std::forward_as_tuple(value.fileID, value.startPosition, value.endPosition, value.symbolID, value.isDefinition);
}

auto get_as_tuple(const BaseOf& value)
{
  return std::forward_as_tuple(value.baseClassID, value.derivedClassID, value.accessSpecifier);
}

auto get_as_tuple(const Override& value)
{
  return std::forward_as_tuple(value.baseMethodID, value.overrideMethodID);
}

template<typename T>
void sortAndRemoveDuplicates(std::vector<T>& list)
{
  std::sort(list.begin(), list.end(), [](const T& a, const T& b) {
    return get_as_tuple(a) > get_as_tuple(b);
    });

  auto it = std::unique(list.begin(), list.end(), [](const T& a, const T& b) {
    return get_as_tuple(a) == get_as_tuple(b);
    });

  list.erase(it, list.end());
}

} // namespace

template<typename T>
class ValueUpdater
{
private:
  std::optional<T>& m_value;
  bool m_invalidated = false;

public:
  explicit ValueUpdater(std::optional<T>& val)
    : m_value(val)
  {

  }

  void update(const T& val)
  {
    if (m_invalidated)
    {
      return;
    }

    if (m_value.has_value())
    {
      if (*m_value != val)
      {
        m_value.reset();
        m_invalidated = true;
      }
    }
    else
    {
      m_value = val;
    }
  }

  void update(const std::optional<T>& val)
  {
    if (val.has_value())
    {
      update(*val);
    }
  }

  bool valid() const
  {
    return m_value.has_value() && !m_invalidated;
  }


};

FileIdTable::FileIdTable()
{
  m_filesmap[""] = 0;
  m_filestable.push_back(m_filesmap.begin()->first);
}

size_t FileIdTable::size() const
{
  return m_filestable.size();
}

FileID FileIdTable::getIdentification(const std::string_view& file)
{
  auto it = m_filesmap.find(file);
  if (it != m_filesmap.end())
  {
    return it->second;
  }

  const auto result = FileID(m_filestable.size());

  bool inserted;
  std::tie(it, inserted) = m_filesmap.insert(std::pair(std::string(file), result));
  assert(inserted);

  m_filestable.push_back(it->first);

  return result;
}

FileID FileIdTable::findIdentification(const std::string_view& file) const
{
  auto it = m_filesmap.find(file);
  assert(it != m_filesmap.end());
  return it->second;
}

std::optional<FileID> FileIdTable::insert(const std::string_view& filePath)
{
  const auto id = FileID(m_filestable.size());

  auto [it, inserted] = m_filesmap.insert(std::pair(std::string(filePath), id));
 
  if (inserted)
  {
    m_filestable.push_back(it->first);
    return id;
  }

  return std::nullopt;
}

std::string_view FileIdTable::getFile(FileID fid) const
{
  return fid < m_filestable.size() ? m_filestable[fid] : std::string_view();
}

void SnapshotMerger::setOutputPath(const std::filesystem::path& outputPath)
{
  m_output_path = outputPath;
}

void SnapshotMerger::addInputPath(const std::filesystem::path& inputPath)
{
  m_input_paths.push_back(inputPath);
}

void SnapshotMerger::setInputs(const std::vector<std::filesystem::path>& inputPaths)
{
  m_input_paths.assign(inputPaths.begin(), inputPaths.end());
}

const std::vector<std::filesystem::path>& SnapshotMerger::inputPaths() const
{
  return m_input_paths;
}

void SnapshotMerger::setProjectHome(const std::filesystem::path& homePath)
{
  m_project_home_path = homePath;
}

void SnapshotMerger::setExtraProperty(const std::string& name, const std::string& value)
{
  m_extra_properties[name] = value;
}

void SnapshotMerger::setFileContentWriter(std::unique_ptr<FileContentWriter> contentWriter)
{
  m_file_content_writer = std::move(contentWriter);
}

void SnapshotMerger::runMerge()
{
  // list good snapshots (remove duplicates and non-snapshot files)
  {
    std::set<std::filesystem::path> filepaths;

    for (const std::filesystem::path& p : m_input_paths)
    {
      auto absolute = std::filesystem::absolute(p);

      if (filepaths.find(absolute.generic_u8string()) != filepaths.end())
      {
        continue;
      }

      SnapshotReader reader;

      if (!reader.open(p)) {
        continue;
      }

      filepaths.insert(absolute.generic_u8string());

      m_snapshots.emplace_back();
      InputSnapshot& snapshot = m_snapshots.back();
      snapshot.reader = std::move(reader);
      snapshot.properties = snapshot.reader.readProperties();
      snapshot.reader.close();
    }
  }

  writer().open(m_output_path);

  // write "info" table
  {
    Snapshot::Properties properties;

    properties["cppscanner.version"] = cppscanner::versioncstr();
    properties["cppscanner.os"] = cppscanner::system_name();

    if (m_project_home_path.has_value())
    {
      properties["project.home"] = Snapshot::normalizedPath(m_project_home_path->generic_u8string());
    }
    else
    {
      std::optional<std::string> home;
      ValueUpdater updater{ home };

      for (InputSnapshot& snapshot : m_snapshots)
      {
        updater.update(snapshot.properties.at("project.home"));
      }

      if (updater.valid())
      {
        properties["project.home"] = Snapshot::normalizedPath(home.value());
      }
      else
      {
        // TODO: print warning ?
      }
    }

    const std::vector<std::string> extraProps{ "scanner.indexLocalSymbols", "scanner.indexExternalFiles", "scanner.root" };
    for (const std::string& key : extraProps)
    {
      std::optional<std::string> value;
      ValueUpdater updater{ value };

      for (InputSnapshot& snapshot : m_snapshots)
      {
        updater.update(getProperty(snapshot.properties, key));
      }

      if (updater.valid())
      {
        properties[key] = *value;
      }
    }

    for (const auto& p : m_extra_properties)
    {
      properties[p.first] = p.second;
    }

    writer().beginTransaction();
    writer().insert(properties);
    writer().endTransaction();
  }

  FileIdTable table;

  // process files from all snapshots: build 'table' and fill "file" table
  {
    struct FileContent
    {
      std::string sha1;
      std::string text;
    };
    std::map<FileID, FileContent> file_content_map; // content of files belonging to the project

    // build the file id table
    {
      std::set<std::string> external_files;

      for (InputSnapshot& snapshot : m_snapshots)
      {
        snapshot.reader.reopen();

        constexpr bool fetch_content = true;
        std::vector<File> files = snapshot.reader.getFiles(fetch_content);

        snapshot.reader.close();

        auto outside_project_it = partitionByProjectStatus(files, snapshot.properties.at("project.home"));

        for (auto it = files.begin(); it != outside_project_it; ++it)
        {
          std::optional<FileID> fileid = table.insert(it->path);

          if (fileid.has_value())
          {
            FileContent& content = file_content_map[*fileid];
            content.text = std::move(it->content);
            content.sha1 = std::move(it->sha1);
          }
        }

        for (auto it = outside_project_it; it != files.end(); ++it)
        {
          external_files.insert(it->path);
        }
      }

      for (const std::string& path : external_files)
      {
        table.insert(path);
      }
    }

    if (m_file_content_writer)
    {
      for (auto& element : file_content_map)
      {
        if (!element.second.text.empty()) {
          continue;
        }

        File f;
        f.id = element.first;
        f.path = table.getFile(f.id);
        
        m_file_content_writer->fill(f);

        element.second.sha1 = f.sha1;
        element.second.text = std::move(f.content);
      }
    }

    // write "file" table
    {
      std::vector<File> files;
      files.reserve(table.size());

      for (size_t i(1); i < table.size(); ++i)
      {
        File f;
        f.id = static_cast<FileID>(i);
        f.path = table.getFile(f.id);
        files.push_back(f);
      }

      writer().beginTransaction();
      writer().insertFilePaths(files);
      writer().endTransaction();

      files.clear();
      for (auto& p : file_content_map)
      {
        File f;
        f.id = p.first;
        f.path = table.getFile(f.id);
        f.sha1 = std::move(p.second.sha1);
        f.content = std::move(p.second.text);
        files.push_back(f);
      }


      writer().beginTransaction();
      writer().insertFiles(files);
      writer().endTransaction();
    }

    file_content_map.clear();
  }

  // write "include" table
  {
    std::vector<Include> allincludes;

    for (InputSnapshot& snapshot : m_snapshots)
    {
      snapshot.reader.reopen();

      constexpr bool fetch_content = false;
      std::vector<File> files = snapshot.reader.getFiles(fetch_content);

      snapshot.idtable = createRemapTable(files, table);

      std::vector<Include> includes = snapshot.reader.getIncludes();

      snapshot.reader.close();

      for (Include& inc : includes)
      {
        inc.fileID = snapshot.idtable[inc.fileID];
        inc.includedFileID = snapshot.idtable[inc.includedFileID];
        allincludes.push_back(inc);
      }
    }

    if (m_snapshots.size() > 1)
    {
      sortAndRemoveDuplicates(allincludes);
    }
    writer().beginTransaction();
    writer().insertIncludes(allincludes);
    writer().endTransaction();
  }
  
  // write "argumentPassedByReference" table
  {
    std::vector<ArgumentPassedByReference> all;

    for (InputSnapshot& snapshot : m_snapshots)
    {
      snapshot.reader.reopen();

      std::vector<ArgumentPassedByReference> annotations = snapshot.reader.getArgumentsPassedByReference();

      snapshot.reader.close();

      for (ArgumentPassedByReference& a : annotations)
      {
        a.fileID = snapshot.idtable[a.fileID];
        all.push_back(a);
      }
    }

    if (m_snapshots.size() > 1)
    {
      sortAndRemoveDuplicates(all);
    }
    writer().beginTransaction();
    writer().insert(all);
    writer().endTransaction();
  }

  // write "diagnostic" table
  {
    std::vector<Diagnostic> all;

    for (InputSnapshot& snapshot : m_snapshots)
    {
      snapshot.reader.reopen();

      std::vector<Diagnostic> diagnostics = snapshot.reader.getDiagnostics();

      snapshot.reader.close();

      for (Diagnostic& d : diagnostics)
      {
        d.fileID = snapshot.idtable[d.fileID];
        all.push_back(d);
      }
    }

    if (m_snapshots.size() > 1)
    {
      sortAndRemoveDuplicates(all);
    }
    writer().beginTransaction();
    writer().insertDiagnostics(all);
    writer().endTransaction();
  }

  // write "symbol" table
  {
    std::map<SymbolID, IndexerSymbol> symbolsmap;

    for (InputSnapshot& snapshot : m_snapshots)
    {
      snapshot.reader.reopen();

      {
        SymbolRecordIterator iterator{ snapshot.reader };

        while (iterator.hasNext())
        {
          SymbolRecord symbol = iterator.next();

          auto it = symbolsmap.find(symbol.id);

          if (it != symbolsmap.end())
          {
            update(it->second, symbol);
          }
          else
          {
            IndexerSymbol& newsymbol = symbolsmap[symbol.id];
            static_cast<SymbolRecord&>(newsymbol) = symbol;
          }
        }
      }

      snapshot.reader.close();
    }

    std::vector<const IndexerSymbol*> all;
    for (const auto& p : symbolsmap)
    {
      all.push_back(&p.second);
    }

    writer().beginTransaction();
    writer().insertSymbols(all);
    writer().endTransaction();
  }

  // write "symbolReference" table
  {
    std::vector<SymbolReference> all;

    for (InputSnapshot& snapshot : m_snapshots)
    {
      snapshot.reader.reopen();

      std::vector<SymbolReference> annotations = snapshot.reader.getSymbolReferences();

      snapshot.reader.close();

      for (SymbolReference& a : annotations)
      {
        a.fileID = snapshot.idtable[a.fileID];
        all.push_back(a);
      }
    }

    if (m_snapshots.size() > 1)
    {
      sortAndRemoveDuplicates(all);
    }
    writer().beginTransaction();
    writer().insert(all);
    writer().endTransaction();
  }

  // write "symbolDeclaration" table
  {
    std::vector<SymbolDeclaration> all;

    for (InputSnapshot& snapshot : m_snapshots)
    {
      snapshot.reader.reopen();

      std::vector<SymbolDeclaration> annotations = snapshot.reader.getSymbolDeclarations();

      snapshot.reader.close();

      for (SymbolDeclaration& a : annotations)
      {
        a.fileID = snapshot.idtable[a.fileID];
        all.push_back(a);
      }
    }

    if (m_snapshots.size() > 1)
    {
      sortAndRemoveDuplicates(all);
    }
    writer().beginTransaction();
    writer().insert(all);
    writer().endTransaction();
  }

  // write "baseOf" table
  {
    std::vector<BaseOf> all;

    for (InputSnapshot& snapshot : m_snapshots)
    {
      snapshot.reader.reopen();

      std::vector<BaseOf> entries = snapshot.reader.getBases();

      snapshot.reader.close();

      all.insert(all.end(), entries.begin(), entries.end());
    }

    if (m_snapshots.size() > 1)
    {
      sortAndRemoveDuplicates(all);
    }
    writer().beginTransaction();
    writer().insertBaseOfs(all);
    writer().endTransaction();
  }

  // write "override" table
  {
    std::vector<Override> all;

    for (InputSnapshot& snapshot : m_snapshots)
    {
      snapshot.reader.reopen();

      std::vector<Override> entries = snapshot.reader.getOverrides();

      snapshot.reader.close();

      all.insert(all.end(), entries.begin(), entries.end());
    }

    if (m_snapshots.size() > 1)
    {
      sortAndRemoveDuplicates(all);
    }
    writer().beginTransaction();
    writer().insertOverrides(all);
    writer().endTransaction();
  }

  // write "enumConstantInfo" table
  writeInfoTable<EnumConstantRecordIterator>();
  // write "enumInfo" table
  writeInfoTable<EnumRecordIterator>();
  // write "functionInfo" table
  writeInfoTable<FunctionRecordIterator>();
  // write "macroInfo" table
  writeInfoTable<MacroRecordIterator>();
  // write "namespaceAliasInfo" table
  writeInfoTable<NamespaceAliasRecordIterator>();
  // write "parameterInfo" table
  writeInfoTable<ParameterRecordIterator>();
  // write "variableInfo" table
  writeInfoTable<VariableRecordIterator>();
}

template<typename RecordIterator>
void SnapshotMerger::writeInfoTable()
{
  using RecordType = typename RecordIterator::value_t;
  using InfoType = typename record_traits<RecordType>::info_t;

  std::map<SymbolID, InfoType> infomap;

  for (InputSnapshot& snapshot : m_snapshots)
  {
    snapshot.reader.reopen();

    {
      RecordIterator iterator{ snapshot.reader };

      while (iterator.hasNext())
      {
        RecordType symbol = iterator.next();
        // TODO: updating might be better than overwriting...
        infomap[symbol.id] = symbol;
      }
    }

    snapshot.reader.close();
  }

  writer().beginTransaction();
  writer().insert(infomap);
  writer().endTransaction();
}

SnapshotWriter& SnapshotMerger::writer()
{
  return m_writer;
}

} // namespace cppscanner

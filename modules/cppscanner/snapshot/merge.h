// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_SNAPSHOT_MERGE_H
#define CPPSCANNER_SNAPSHOT_MERGE_H

#include "snapshotreader.h"
#include "snapshotwriter.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace cppscanner
{

class FileIdTable
{
private:
  std::map<std::string, FileID, std::less<>> m_filesmap;
  std::vector<std::string_view> m_filestable;

public:

  FileIdTable();

  size_t size() const;

  FileID getIdentification(const std::string_view& file);
  std::optional<FileID> insert(const std::string_view& filePath);
  FileID findIdentification(const std::string_view& file) const;

  std::string_view getFile(FileID fid) const;
};

class FileContentWriter
{
public:
  virtual ~FileContentWriter() = default;

  virtual void fill(File& file) = 0;
};

class SnapshotMerger
{
public:

  void setOutputPath(const std::filesystem::path& outputPath);
  void addInputPath(const std::filesystem::path& inputPath);
  void setInputs(const std::vector<std::filesystem::path>& inputPaths);
  void setProjectHome(const std::filesystem::path& homePath);
  void setExtraProperty(const std::string& name, const std::string& value);
  void setFileContentWriter(std::unique_ptr<FileContentWriter> contentWriter);

  const std::vector<std::filesystem::path>& inputPaths() const;

  void runMerge();

private:
  SnapshotWriter& writer();

  template<typename RecordIterator>
  void writeInfoTable();

private:
  struct InputSnapshot
  {
    SnapshotReader reader;
    Snapshot::Properties properties;
    std::vector<FileID> idtable;
  };

private:
  std::vector<std::filesystem::path> m_input_paths;
  std::filesystem::path m_output_path;
  std::optional<std::filesystem::path> m_project_home_path;
  Snapshot::Properties m_extra_properties;
  std::vector<InputSnapshot> m_snapshots;
  SnapshotWriter m_writer;
  std::unique_ptr<FileContentWriter> m_file_content_writer;
};

} // namespace cppscanner

#endif // CPPSCANNER_SNAPSHOT_MERGE_H

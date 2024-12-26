// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_SNAPSHOTCREATOR_H
#define CPPSCANNER_SNAPSHOTCREATOR_H

#include "cppscanner/snapshot/snapshotwriter.h"

#include <filesystem>
#include <string>
#include <vector>

namespace cppscanner
{

class FileIdentificator;
class TranslationUnitIndex;

struct SnapshotCreatorData;

class SnapshotCreator
{
public:
  SnapshotCreator() = delete;
  SnapshotCreator(const SnapshotCreator&) = delete;
  ~SnapshotCreator();

  explicit SnapshotCreator(FileIdentificator& fileIdentificator);

  FileIdentificator& fileIdentificator() const;

  void setHomeDir(const std::filesystem::path& p);
  void setCaptureFileContent(bool on = true);

  void init(const std::filesystem::path& dbPath);

  SnapshotWriter* snapshotWriter() const;

  void writeProperty(const std::string& name, const std::string& value);

  void feed(const TranslationUnitIndex& tuIndex);
  void feed(TranslationUnitIndex&& tuIndex);

  void close();

  static void fillContent(File& file);

protected:
  void writeHomeProperty();
  bool fileAlreadyIndexed(FileID f) const;
  void setFileIndexed(FileID f);

private:
  FileIdentificator& m_fileIdentificator;
  std::unique_ptr<SnapshotCreatorData> d;
  std::unique_ptr<SnapshotWriter> m_snapshot;
};

} // namespace cppscanner

#endif // CPPSCANNER_SNAPSHOTCREATOR_H

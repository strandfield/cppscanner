// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#include "fileidentificator.h"

#include <map>
#include <mutex>

namespace cppscanner
{

class BasicFileIdentificator : public FileIdentificator
{
private:
  std::map<std::string, FileID> m_files;

public:
  explicit BasicFileIdentificator(std::map<std::string, FileID> files = {});

  FileID getIdentification(const std::string& file) final;
  std::vector<std::string> getFiles() const final;
};

BasicFileIdentificator::BasicFileIdentificator(std::map<std::string, FileID> files)
  : m_files(std::move(files))
{
  m_files[""] = 0;
}

FileID BasicFileIdentificator::getIdentification(const std::string& file)
{
  auto it = m_files.find(file);

  if (it != m_files.end()) {
    return it->second;
  }

  auto result = FileID(m_files.size());
  m_files[file] = result;
  return result;
}

std::vector<std::string> BasicFileIdentificator::getFiles() const
{
  std::vector<std::string> result;
  result.resize(m_files.size());

  for (const auto& p : m_files) {
    result[p.second] = p.first;
  }

  return result;
}

} // namespace cppscanner

namespace cppscanner
{

class ThreadSafeFileIdentificator : public FileIdentificator
{
private:
  BasicFileIdentificator m_files;
  mutable std::mutex m_mutex;

public:
  explicit ThreadSafeFileIdentificator(std::map<std::string, FileID> files = {});

  FileID getIdentification(const std::string& file) final;
  std::vector<std::string> getFiles() const final;
};

ThreadSafeFileIdentificator::ThreadSafeFileIdentificator(std::map<std::string, FileID> files)
  : m_files(std::move(files))
{

}

FileID ThreadSafeFileIdentificator::getIdentification(const std::string& file)
{
  std::lock_guard lock{ m_mutex };
  return m_files.getIdentification(file);
}

std::vector<std::string> ThreadSafeFileIdentificator::getFiles() const
{
  std::lock_guard lock{ m_mutex };
  return m_files.getFiles();
}

} // namespace cppscanner

namespace cppscanner
{

FileIdentificator::~FileIdentificator()
{

}

std::string FileIdentificator::getFile(FileID fid) const
{
  return getFiles().at(fid);
}

std::unique_ptr<FileIdentificator> FileIdentificator::createFileIdentificator()
{
  return std::make_unique<BasicFileIdentificator>();
}

std::unique_ptr<FileIdentificator> FileIdentificator::createThreadSafeFileIdentificator()
{
  return std::make_unique<ThreadSafeFileIdentificator>();
}

} // namespace cppscanner

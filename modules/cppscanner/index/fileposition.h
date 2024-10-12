// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_FILEPOSITION_H
#define CPPSCANNER_FILEPOSITION_H

#include <cstdint>

namespace cppscanner
{

/**
 * \brief represents a position (line,column) within a file
 */
class FilePosition 
{
private:
  uint32_t m_pack;

public:
  FilePosition() = default;
  FilePosition(const FilePosition&) = default;

  static constexpr int ColumnBits = 12;
  static constexpr int MaxLine = (1 << (32 - ColumnBits)) - 1;
  static constexpr int MaxColumn = (1 << ColumnBits) - 1;

  FilePosition(int l, int c);

  int line() const;
  int column() const;

  void setLine(int l);
  void setColumn(int c);

  bool overflows() const;

  uint32_t bits() const;
};

inline FilePosition::FilePosition(int l, int c)
{
  if (l > MaxLine) {
    l = MaxLine;
  }

  if (c > MaxColumn) {
    c = MaxColumn;
  }

  m_pack = (l << ColumnBits) | c;
}

inline int FilePosition::line() const
{
  return m_pack >> ColumnBits;
}

inline int FilePosition::column() const
{
  return m_pack & MaxColumn;
}

inline void FilePosition::setLine(int l)
{
  if (l > MaxLine) {
    l = MaxLine;
  }

  m_pack = (l << ColumnBits) | column();
}

inline void FilePosition::setColumn(int c)
{
  if (c > MaxColumn) {
    c = MaxColumn;
  }

  m_pack = (m_pack & ~MaxColumn) | c;
}

inline bool FilePosition::overflows() const
{
  return line() == MaxLine || column() == MaxColumn;
}

inline uint32_t FilePosition::bits() const
{
  return m_pack;
}

inline bool operator==(const FilePosition& lhs, const FilePosition& rhs) {
  return lhs.line() == rhs.line() && lhs.column() == rhs.column();
}

inline bool operator<(const FilePosition& lhs, const FilePosition& rhs) {
  return lhs.line() < rhs.line() || (lhs.line() == rhs.line() && lhs.column() < rhs.column());
}

} // namespace cppscanner

#endif // CPPSCANNER_FILEPOSITION_H

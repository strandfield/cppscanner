// Copyright (C) 2024 Vincent Chambrin
// This file is part of the 'cppscanner' project.
// For conditions of distribution and use, see copyright notice in LICENSE.

#ifndef CPPSCANNER_SYMBOLID_H
#define CPPSCANNER_SYMBOLID_H

#include <cstdint>
#include <string>

namespace cppscanner
{

class SymbolID
{
private:
  uint64_t m_id = 0;

public:
  SymbolID() = default;

  typedef uint64_t value_type;
  uint64_t rawID() const;

  bool isValid() const;

  std::string toHex() const;
  
  static SymbolID fromRawID(uint64_t id);

  operator bool() const;

protected:
  explicit SymbolID(uint64_t id);
};

inline SymbolID::SymbolID(uint64_t id) :
  m_id(id)
{

}

inline uint64_t SymbolID::rawID() const
{
  return m_id;
}

inline bool SymbolID::isValid() const
{
  return rawID() != 0;
}

inline SymbolID SymbolID::fromRawID(uint64_t id)
{
  return SymbolID{ id };
}

inline SymbolID::operator bool() const
{
  return isValid();
}

inline bool operator==(const SymbolID& lhs, const SymbolID& rhs)
{
  return lhs.rawID() == rhs.rawID();
}

inline bool operator!=(const SymbolID& lhs, const SymbolID& rhs)
{
  return !(lhs == rhs);
}

inline bool operator<(const SymbolID & lhs, const SymbolID & rhs)
{
  return lhs.rawID() < rhs.rawID();
}

} // namespace cppscanner

#endif // CPPSCANNER_SYMBOLID_H

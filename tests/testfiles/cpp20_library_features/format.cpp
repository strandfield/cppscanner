
#include "format.h"

#include <algorithm>
#include <format>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <sstream>
#include <string_view>

// the following example is taken from https://en.cppreference.com/w/cpp/utility/format/formatter

struct QuotableString : std::string_view {};

template<>
struct std::formatter<QuotableString, char>
{
  bool quoted = false;

  template<class ParseContext>
  constexpr ParseContext::iterator parse(ParseContext& ctx)
  {
    auto it = ctx.begin();
    if (it == ctx.end())
      return it;

    if (*it == '#')
    {
      quoted = true;
      ++it;
    }
    if (it != ctx.end() && *it != '}')
      throw std::format_error("Invalid format args for QuotableString.");

    return it;
  }

  template<class FmtContext>
  FmtContext::iterator format(QuotableString s, FmtContext& ctx) const
  {
    std::ostringstream out;
    if (quoted)
      out << std::quoted(s);
    else
      out << s;

    return std::ranges::copy(std::move(out).str(), ctx.out()).out;
  }
};

void testFormatLibrary()
{
  std::string text = std::format("Hello {}!", "World");
  std::cout << text << std::endl;

  std::cout << std::format("pi is {}", std::numbers::pi_v<double>) << std::endl;

  text = std::format("{0} and {0} is {1}", 2, 4);
  std::cout << text << std::endl;

  QuotableString quotable{ "great" };
  std::cout << std::format("is it {0}, or {0:#}?", quotable) << std::endl;
}

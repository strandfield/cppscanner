
#include <variant>

struct VariantVisitor
{
  void operator()(char c) { }
  void operator()(bool b) { }
  void operator()(int n) { }
  void operator()(double x) { }
};

void hello_variant(const std::variant<char, bool, int, double>& value)
{
  if (!std::holds_alternative<bool>(value) || std::get<bool>(value)) {
    std::visit(VariantVisitor(), value);
  }
}

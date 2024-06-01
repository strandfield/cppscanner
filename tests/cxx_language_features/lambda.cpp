

void functionWithLambda(int& a, int b = 1)
{
  auto add_to_a = [&a](int c) -> void {
    a += c;
    };
  add_to_a(b);
}

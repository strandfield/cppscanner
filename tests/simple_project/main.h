
#ifndef MAIN_H
#define MAIN_H

enum class ColorChannel {
  RED,
  GREEN,
  BLUE
};

struct Foo
{
  int a;
  static const bool b;
};

class Base
{
public:
  virtual void vmethod() = 0;
};

class Derived : public Base
{
protected:
  void vmethod() override;
};

namespace foobar {
void qux();
}

#endif MAIN_H
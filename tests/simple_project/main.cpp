
#include "main.h"

const bool Foo::b = false;

static bool gBoolean = false;

void Derived::vmethod()
{
  gBoolean = !Foo::b;
}

int main(int argc, char* argv[])
{
  Foo myfoo;
  myfoo.a = 5;

  return 0;
}

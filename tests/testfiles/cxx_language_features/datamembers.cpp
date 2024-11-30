
struct Foo
{
  int a;
  static const bool b;
};

const bool Foo::b = false;

void dataMembersFunc()
{
  Foo myfoo;
  myfoo.a = 5;
}

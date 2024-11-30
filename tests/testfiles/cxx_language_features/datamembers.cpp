
struct Foo
{
  int a;
  static const bool b;
};

const bool Foo::b = false;

int main()
{
  Foo myfoo;
  myfoo.a = 5;
}

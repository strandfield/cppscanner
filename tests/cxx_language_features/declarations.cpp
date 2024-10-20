
typedef int integer_t;

using real = double;

enum Alphabet
{
  A,B,C,D,E,F
};

class MyClass;

class MyClass
{
public:

  void myMethod();

  void myInlineMethod()
  {

  }

  static const int N;
  static constexpr int M = 42;
};

void MyClass::myMethod()
{

}

const int MyClass::N = 43;

static constexpr int P = 44;

int myFunctionDecl(int, int);

int myFunctionDecl(int a, int b = 0);

int myFunctionDecl(int a, int b)
{
  return a * b;
}

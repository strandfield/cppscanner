
class ClassWithConvertingConstructor
{
  int m_value = 0;
public:

  // attention: we do not mark the constructor as explicit!
  ClassWithConvertingConstructor(int v) 
    : m_value(v) { }
};

void fConvertingCtor(ClassWithConvertingConstructor valueHolder)
{
  (void)valueHolder;
}

void callingFConvertingCtor() {
  fConvertingCtor(1); // ctor implicitly called
  fConvertingCtor({2}); // ctor implicitly called
  fConvertingCtor(ClassWithConvertingConstructor(3)); // ctor called, class implicitly referenced
}

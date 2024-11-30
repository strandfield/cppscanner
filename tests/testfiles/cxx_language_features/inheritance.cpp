
static bool gBoolean = false;

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

void Derived::vmethod()
{
  gBoolean = !gBoolean;
}

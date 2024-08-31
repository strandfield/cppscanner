
struct MyAggregate
{
  bool flag;
  int n;
  float x;
};

void init_aggregate()
{
  MyAggregate a{ .flag = false, .n = 5 };
  MyAggregate b{ .flag = false, .x = 3.14f };
  MyAggregate c{ .x = 3.14f };

  (void)a;
  (void)b;
  (void)c;
}


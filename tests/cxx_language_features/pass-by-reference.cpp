
void takingParamsByReference(int& a, int b, int c)
{
  a = b + c;
}

void passingArgumentByReference()
{
  int n = 0;
  takingParamsByReference(n, 1, 2);
}

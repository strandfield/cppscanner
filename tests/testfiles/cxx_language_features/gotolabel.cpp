

int gotolabel(int n)
{
  if (n < 0) {
    goto labelB;
  }

  if ((n % 2) == 0) {
    goto labelA;
  }

  n += 1;

labelA:
  return n;

labelB:
  return -1;
}

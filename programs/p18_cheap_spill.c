int p18_cheap_spill(int a, int b, int c) {
  int x = a + 1;
  int y = b + 1;
  int z = c + 1;
  int t0 = x + y;
  int cheap = z + 0;
  int t1 = t0 + z;
  return t1 + cheap;
}
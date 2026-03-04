int p14_cross_reuse(int a, int b, int c) {
  int x = a + 1;
  int y = b + 2;
  int z = c + 3;
  int u = x + y;
  int v = y + z;
  int w = u + v;
  return w + x + z;
}
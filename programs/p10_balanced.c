int p10_balanced(int a, int b, int c, int d) {
  int x = a + b;
  int y = c + d;
  int z = x + y;
  int u = z + x;
  int v = z + y;
  return u + v;
}
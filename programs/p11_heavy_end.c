int p11_heavy_end(int a, int b, int c) {
  int x = a + 1;
  int y = b + 2;
  int z = c + 3;
  int u = x + y;
  int v = y + z;
  int w = z + x;
  return x + y + z + u + v + w;
}
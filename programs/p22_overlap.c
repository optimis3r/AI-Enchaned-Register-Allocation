int p22_overlap(int a,int b,int c) {
  int x0 = a + 1;
  int x1 = x0 + 1;
  int x2 = x1 + 1;
  int y0 = b + 1;
  int y1 = y0 + x1;
  int z0 = c + y1;
  return z0 + x2;
}
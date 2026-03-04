int p37_heavy_return(int a,int b,int c,int d) {
  int x0 = a + 1;
  int x1 = b + 2;
  int x2 = c + 3;
  int x3 = d + 4;
  int y0 = x0 + x1;
  int y1 = x2 + x3;
  int z0 = y0 + y1;
  return x0 + x1 + x2 + x3 + y0 + y1 + z0;
}
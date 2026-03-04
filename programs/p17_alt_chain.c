int p17_alt_chain(int a) {
  int x0 = a + 1;
  int x1 = x0 + 2;
  int x2 = x0 + 3;
  int x3 = x1 + x2;
  int x4 = x3 + x2;
  return x4 + x1;
}
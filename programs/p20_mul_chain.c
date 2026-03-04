int p20_mul_chain(int a) {
  int x0 = a + 2;
  int x1 = x0 * 2;
  int x2 = x1 * 2;
  int x3 = x2 + 5;
  return x3 + x1;
}
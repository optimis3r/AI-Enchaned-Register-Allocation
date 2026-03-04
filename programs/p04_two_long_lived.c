int p04_two_long_lived(int a, int b) {
  int A = a + 1;
  int B = b + 2;
  int t0 = A + 10;
  int t1 = B + 20;
  int t2 = t0 + t1;
  int t3 = A + t2;
  int t4 = B + t3;
  return A + B + t4;
}

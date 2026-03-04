int p15_mid_pressure(int a, int b, int c) {
  int v0 = a + 1;
  int v1 = b + 1;
  int v2 = c + 1;
  int v3 = v0 + v1;
  int v4 = v2 + v1;
  return v3 + v4 + v0 + v2;
}
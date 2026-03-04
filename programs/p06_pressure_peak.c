int p06_pressure_peak(int a, int b, int c) {
  int v0 = a + 1;
  int v1 = b + 2;
  int v2 = c + 3;
  int v3 = v0 + v1;
  int v4 = v1 + v2;
  int v5 = v0 + v2;
  int sum = v0 + v1 + v2 + v3 + v4 + v5;
  return sum;
}

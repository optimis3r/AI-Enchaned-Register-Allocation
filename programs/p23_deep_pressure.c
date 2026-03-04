int p23_deep_pressure(int a,int b,int c) {
  int v0 = a + 1;
  int v1 = b + 2;
  int v2 = c + 3;
  int v3 = v0 + v1;
  int v4 = v1 + v2;
  int v5 = v0 + v2;
  int v6 = v3 + v4;
  int v7 = v6 + v5;
  int sum = v0 + v1 + v2 + v3 + v4 + v5 + v6 + v7;
  return sum;
}
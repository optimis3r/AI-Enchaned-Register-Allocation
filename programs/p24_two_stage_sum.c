int p24_two_stage_sum(int a,int b,int c,int d) {
  int x0 = a + 1;
  int x1 = b + 2;
  int x2 = c + 3;
  int x3 = d + 4;
  int s0 = x0 + x1 + x2 + x3;
  int s1 = s0 + x0 + x3;
  return s1;
}
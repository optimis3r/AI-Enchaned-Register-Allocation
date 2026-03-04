int p13_two_chains_join(int a, int b) {
  int p0 = a + 1;
  int p1 = p0 + 2;
  int p2 = p1 + 3;
  int q0 = b + 1;
  int q1 = q0 + 2;
  int q2 = q1 + 3;
  int r = p2 + q2;
  return r + p1 + q1;
}
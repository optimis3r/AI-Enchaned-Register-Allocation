int p02_many_temps(int a, int b) {
  int t0 = a + b;
  int t1 = a - b;
  int t2 = a * 2;
  int t3 = b * 3;
  int t4 = t0 + t1;
  int t5 = t2 + t3;
  return t4 + t5 + t0;
}

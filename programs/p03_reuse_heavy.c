int p03_reuse_heavy(int a) {
  int hot = a + 7;
  int x = hot + 1;
  int y = hot + 2;
  int z = hot + 3;
  int w = hot + 4;
  return x + y + z + w + hot;
}

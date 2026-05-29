#include "test.h"

// Flexible array member (C99) — int data[] as the last struct member
struct FAM {
  int len;
  int data[];
};

int main() {
  char buf[64];
  struct FAM *f = (struct FAM *)buf;
  int n = 5;
  f->len = n;
  for (int i = 0; i < n; i++)
    f->data[i] = (i + 1) * 100;

  ASSERT(5, f->len);
  ASSERT(100, f->data[0]);
  ASSERT(500, f->data[4]);

  printf("OK\n");
  return 0;
}

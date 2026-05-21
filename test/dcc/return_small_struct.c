#include "test.h"

struct A {
  short c[2];
};

struct A ret_struct(int p) {
  struct A r = {.c[0] = 1};
  r.c[1] = p;
  return r;
}

int main() {
  struct A a = ret_struct(3);
  ASSERT(1, a.c[0]);
  ASSERT(3, a.c[1]);
  int v = ret_struct(4).c[1] = 3;
  ASSERT(3, v);
  a = ret_struct(6);
  ASSERT(1, a.c[0]);
  ASSERT(6, a.c[1]);
  printf("OK\n");
}
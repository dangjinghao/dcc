#include "test.h"

static struct S {
  char a;
  char b;
  long c;
  struct {
    int i;
    long x;
    union {
      int v;
      char a[4];
    } u;
  } s;
  short d;
  int e;
} sa = {.a = 1, .b = 2, .c = 3, .d = 4, .e = 5, .s.i = 1, .s.u.a[0] = 1};

int main() {
  // Top-level members
  ASSERT(1, sa.a);
  ASSERT(2, sa.b);
  ASSERT(3, (int)(long)sa.c);
  ASSERT(4, (int)sa.d);
  ASSERT(5, sa.e);

  // Nested struct members
  ASSERT(1, sa.s.i);
  ASSERT(0, (int)(long)sa.s.x);    // not initialized

  // Union members (u.a[0] = 1 → u.v sees the same byte as low byte)
  ASSERT(1, sa.s.u.v);             // low byte = 1
  ASSERT(1, sa.s.u.a[0]);
  ASSERT(0, sa.s.u.a[1]);
  ASSERT(0, sa.s.u.a[2]);
  ASSERT(0, sa.s.u.a[3]);

  // Verify sizeof
  ASSERT((int)sizeof(sa) > 16, 1);  // large enough

  printf("OK\n");
  return 0;
}

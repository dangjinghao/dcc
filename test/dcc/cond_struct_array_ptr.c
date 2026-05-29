#include "test.h"

struct S {
  int a;
} *s1[5], *s2[5];

int main() {
  int a = 0, b = 1;

  struct S s2m = {.a = 42};
  s2[1] = &s2m;
  // Ternary selects s2, subscript [1], deref, read a
  struct S *x = (b > a ? s2 : s1)[1];
  ASSERT(42, x->a);

  // Ternary selects s1 (false condition), s1[1] is NULL (not set)
  int false_cond = 0;
  void *p = (false_cond ? s2 : s1)[1];
  ASSERT(1, (int)(p == 0));
  // Ternary selects s2 (true condition), s2[1] is non-NULL
  int true_cond = 1;
  p = (true_cond ? s2 : s1)[1];
  ASSERT(1, (int)(p != 0));

  printf("OK\n");
  return 0;
}

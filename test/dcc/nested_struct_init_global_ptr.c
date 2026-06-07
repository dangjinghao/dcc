#include "test.h"

// C11 6.7.9, nested struct member access as a global initializer.
// `g30.a.a` is a member-access expression that refers to an array
// within a nested struct.  Used as an initializer for a pointer,
// the array decays to a pointer to its first element.

struct {
  struct {
    int a[3];
  } a;
} g30 = {{{1, 2, 3}}};

int *g31 = g30.a.a;

int main() {
  ASSERT(1, g31[0]);
  ASSERT(2, g31[1]);
  ASSERT(3, g31[2]);

  // pointer derived from a single-level struct member that is an array
  struct {
    int a[3];
  } s = {{4, 5, 6}};
  int *p = s.a;
  ASSERT(4, p[0]);
  ASSERT(5, p[1]);
  ASSERT(6, p[2]);

  printf("OK\n");
  return 0;
}

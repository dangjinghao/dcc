#include "test.h"
int a[2][3] = {{1, 2, 3}, {4, 5, 6}};
int main() {
  // Test 1: 2D array dereferencing
  ASSERT(1, *a[0]);       // a[0][0]
  ASSERT(1, a[0][0]);     // direct access
  ASSERT(1, *(*a));       // **a should equal a[0][0]
  ASSERT(4, *(*(a + 1))); // a[1][0]
  // Test 2: Global array
  ASSERT(1, **a);
  // Test 3: Array decay in assignment
  int (*p)[3] = a;
  ASSERT(1, *p[0]);
  ASSERT(1, (*p)[0]);
  printf("OK\n");
  return 0;
}

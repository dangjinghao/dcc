#include "test.h"

int main() {
  int n = 2;
  int m = 3;
  int a[n][m];

  a[0][0] = 10;
  a[0][1] = 11;
  a[0][2] = 12;
  a[1][0] = 20;
  a[1][1] = 21;
  a[1][2] = 22;

  int (*p)[m] = a;

  ASSERT(10, p[0][0]);
  ASSERT(12, p[0][2]);

  p += 1;
  ASSERT(20, p[0][0]);
  ASSERT(22, p[0][2]);

  p -= 1;
  ASSERT(10, p[0][0]);
  ASSERT(11, p[0][1]);

  printf("OK\n");
}
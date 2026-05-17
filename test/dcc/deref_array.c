#include "test.h"
int x[2][3] = {{1, 2, 3}, {4, 5, 6}};
int main() {
  ASSERT(1, **x);
  printf("OK\n");
  return 0;
}

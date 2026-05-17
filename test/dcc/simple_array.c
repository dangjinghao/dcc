#include "test.h"
int x[3] = {1, 2, 3};
int main() {
  ASSERT(0, *(x + 1) - 2);
  printf("OK\n");
}

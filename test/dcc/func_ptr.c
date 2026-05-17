#include "test.h"
int F() { return 42; }

int main() {
  ASSERT(42, (*************F)());
  ASSERT(42, (**&*F)());
  ASSERT(42, (&F)());
  ASSERT(42, (*&F)());
  ASSERT(42, (***&F)());
  printf("OK\n");
}
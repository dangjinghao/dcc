#include "test.h"
int global_val = 10;

int main() {
  ASSERT(10, global_val);
  asm("movl $42, global_val(%rip)");

  ASSERT(42, global_val);
  printf("OK\n");
  return 0;
}
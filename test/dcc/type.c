#include "test.h"

int main() {
  ASSERT(1, sizeof(int) == sizeof(~(char)1));
  ASSERT(1, sizeof(long) == sizeof(~(long)1));
  ASSERT(1, sizeof(int) == sizeof(~(char)1 >> 1));
  ASSERT(1, sizeof(long) == sizeof(~(long)1 >> 1));
  printf("OK\n");
}

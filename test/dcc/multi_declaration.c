#include "test.h"

typedef int calc_t(int);

calc_t calc1, calc2;

int main() {
  ASSERT(2, calc1(1));
  ASSERT(6, calc2(4));
  printf("ok\n");
}

int calc1(int i) { return 1 + i; }

int calc2(int i) { return 2 + i; }

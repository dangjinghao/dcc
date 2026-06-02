#include "test.h"

// lua-5.5.0/luac.c

static char v[] = {"str"};

int main() {
  ASSERT('r', v[2]);
  ASSERT(3, strlen(v));
  ASSERT(0, strcmp(v, "str"));
  printf("ok\n");
  return 0;
}
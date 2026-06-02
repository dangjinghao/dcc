#include "test.h"
extern int sscanf(const char *__restrict __s, const char *__restrict __format,
                  ...) __asm__(""
                               "__isoc99_sscanf")
    __attribute__((__nothrow__, __leaf__));

int main() {
  char *s = "abc 123";
  char v1[32] = {};
  int v2 = 0;
  sscanf(s, "%s %d", v1, &v2);
  ASSERT(0, strcmp(v1, "abc"));
  ASSERT(123, v2);
  printf("ok\n");
}
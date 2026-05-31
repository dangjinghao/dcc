#include "test.h"
#include <limits.h>
#include <stdarg.h>

static void F1(int n, ...) {
  va_list ap;
  va_start(ap, n);

  // a
  int v = va_arg(ap, int);
  ASSERT(-1, v);
  // b
  v = va_arg(ap, int);
  ASSERT(255, v);
  // c
  v = va_arg(ap, int);
  ASSERT(-1, v);
  // d
  v = va_arg(ap, int);
  ASSERT(65535, v);
  // e
  long l = va_arg(ap, long);
  ASSERT(1, l == 9223372036854775807L);
  // f
  unsigned long ul = va_arg(ap, unsigned long);
  ASSERT(1, ul == (9223372036854775807L * 2UL + 1UL));
  // g
  double vf = va_arg(ap, double);
  ASSERT(1, vf == 123.f);
  // h
  vf = va_arg(ap, double);
  ASSERT(1, vf == 12345678.f);
  va_end(ap);
}

int main() {
  // char promoted to int
  char a = -1;
  unsigned char b = 255;
  short c = -1;
  unsigned short d = 65535;
  long e = 9223372036854775807L;
  unsigned long f = (9223372036854775807L * 2UL + 1UL);
  float g = 123.f;
  double h = 12345678.;
  F1(1, a, b, c, d, e, f, g, h);
  printf("OK\n");
  return 0;
}

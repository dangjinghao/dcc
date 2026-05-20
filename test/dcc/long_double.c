#include "test.h"

long double add_ld(long double a, long double b, long double c) {
  return a + b + c;
}

int main() {
  ASSERT(16, sizeof(long double));
  ASSERT(16, _Alignof(long double));

  {
    long double a = 3.0L;
    long double b = 2.0L;

    ASSERT(1, a + b == 5.0L); // 3 + 2
    ASSERT(1, a - b == 1.0L); // 3 - 2
    ASSERT(1, a * b == 6.0L); // 3 * 2
    ASSERT(1, a / b == 1.5L); // 3 / 2
  }

  {
    long double a = 4.0L;
    ASSERT(1, -a == -4.0L);
  }

  {
    long double a = 1.0L;
    long double b = 2.0L;

    ASSERT(1, a < b);
    ASSERT(1, a <= b);
    ASSERT(1, b > a);
    ASSERT(1, b >= a);
    ASSERT(1, a == 1.0L);
    ASSERT(1, a != b);
  }

  {
    int x = 42;
    long double ld = x; // int -> long double
    int y = ld;         // long double -> int
    ASSERT(42, y);

    // round-trip
    long double pi = 3.14159265358979323846L;
    int pi_int = pi;
    ASSERT(3, pi_int);
  }

  {
    double d = 2.718281828;
    long double ld = d; // double -> long double (promotion)
    double d2 = ld;     // long double -> double (truncation)
    ASSERT(1, d == d2);
  }

  {
    long n = 123456789L;
    long double ld = n; // long -> long double
    long m = ld;        // long double -> long
    ASSERT(1, n == m);
  }

  {
    ASSERT(1, add_ld(1.0L, 2.0L, 3.0L) == 6.0L);
  }

  printf("OK\n");
  return 0;
}

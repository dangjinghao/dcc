#include "test.h"

// Zero-length array (GNU extension via cpp)
struct ZLA {
  int len;
  int data[0];
};

int main() {
  // Use a stack buffer large enough for the struct + extra ints
  char buf[64];
  struct ZLA *z = (struct ZLA *)buf;
  int n = 5;
  z->len = n;
  for (int i = 0; i < n; i++)
    z->data[i] = (i + 1) * 10;

  ASSERT(5, z->len);
  ASSERT(10, z->data[0]);
  ASSERT(50, z->data[4]);

  printf("OK\n");
  return 0;
}

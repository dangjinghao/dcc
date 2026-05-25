#include "test.h"

int main() {
  int a = 10, b = 20;
  typedef char vla_ty[a][b];
  vla_ty nv;
  ASSERT(200, sizeof(nv));
  a = 20;
  b = 20;
  ASSERT(200, sizeof(nv));
  printf("OK\n");
}
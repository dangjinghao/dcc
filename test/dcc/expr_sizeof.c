#include "test.h"


int main() {
  ASSERT(sizeof(int), sizeof(~(char)1));
  ASSERT(sizeof(long), sizeof(~(long)1));
  ASSERT(sizeof(int), sizeof(~(char)1 >> 1));
  ASSERT(sizeof(long), sizeof(~(long)1 >> 1));
  // decay
  ASSERT(5 * sizeof(short), sizeof(short[5]));
  ASSERT(sizeof(void *), sizeof(({
           short v[5];
           v;
         })));
  ASSERT(sizeof(void *), sizeof(({
           short v[3];
           v + 1;
         })));
  ASSERT(1, sizeof(main));
  ASSERT(1, sizeof(*main));
  ASSERT(1, sizeof(*************main));
  ASSERT(sizeof(void *), sizeof(&main));
  ASSERT(sizeof(void *), sizeof(&*main));
  ASSERT(1, sizeof(*&main));
  ASSERT(sizeof(void *), sizeof(main + 1));

  printf("OK\n");
}

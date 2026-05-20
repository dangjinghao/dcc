#include "test.h"
int main() {
  int x = 0;

  if (x == 0 && ({
                  int i = 1;
                  while (i <= 10)
                    i *= 2;
                  i;
                }) == 16)
    ;
  else {
    ASSERT(0, 1);
  }
  if (x == 0 || ({
        ASSERT(0, 1);
        1;
      }))
    ;
  else {
    ASSERT(0, 1);
  }
  if ((x == 1 || ({
                   int sum = 0;
                   for (int j = 1; j <= 100; j++) {
                     sum += j;
                   }
                   sum;
                 }) == 5050) &&
      (({
         int sum = 0;
         for (int j = 1; j <= 100; j++) {
           sum += j;
         }
         sum;
       }) != 0))
    ;
  else {
    ASSERT(0, 1);
  }
  printf("OK\n");
}
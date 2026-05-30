#include "test.h"
int array[] = {1, 2, 3, 4, 5, 6};

int *arrp = &array[1];

int main() {
  ASSERT(2, *arrp);
  ASSERT(1, arrp - array);
  ASSERT(4, 3 + arrp - array);
}
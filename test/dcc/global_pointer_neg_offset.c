#include "test.h"

// Negative byte offsets in global pointer initializers where the
// target is not char* (sizeof(pointee) > 1).  dcc used to compute
// GEP indices via (uint64_t)eval_val / pointee_size, which destroyed
// the sign for negative offsets when pointee_size > 1.

// Type that is larger than 1 byte so the division can lose sign.
typedef struct {
  long a, b, c, d, e, f;
} Big;
Big big_arr[10];

// Negative offset into big_arr, expressed in bytes.
// eval_val = -sizeof(Big), pointee_size = sizeof(Big) = 48
// Old code: (uint64_t)(-48) / 48 = huge positive → wrong GEP
Big *bp1 = (Big *)((char *)&big_arr[0] - 48);
Big *bp2 = (Big *)((char *)&big_arr[1] - 48);
Big *bp3 = (Big *)((char *)&big_arr[2] - 96);
Big *bp4 = (Big *)((char *)&big_arr[0] - 8);

// Extract from CPython's obmalloc.c: self-referencing pointer array
// where each entry points 16 bytes BEFORE itself using sizeof(void*).
typedef void *poolp;
#define PTA(x) ((poolp)((char *)&(pools[2 * (x)]) - 2 * sizeof(void *)))
#define PT(x) PTA(x), PTA(x)
static poolp pools[8] = {PT(0), PT(1), PT(2), PT(3)};

int main() {
  ASSERT(1, bp1 == (Big *)((char *)&big_arr[0] - 48));
  ASSERT(1, bp2 == (Big *)((char *)&big_arr[1] - 48));
  ASSERT(1, bp3 == (Big *)((char *)&big_arr[2] - 96));
  ASSERT(1, bp4 == (Big *)((char *)&big_arr[0] - 8));

  // PTA(0): pools[0] and pools[1] = &pools[0] - 2*sizeof(void*)
  ASSERT(1, pools[0] == (poolp)((char *)&pools[0] - 2 * sizeof(void *)));
  ASSERT(1, pools[1] == (poolp)((char *)&pools[0] - 2 * sizeof(void *)));
  // PTA(1): pools[2] and pools[3] = &pools[2] - 2*sizeof(void*)
  ASSERT(1, pools[2] == (poolp)((char *)&pools[2] - 2 * sizeof(void *)));
  ASSERT(1, pools[3] == (poolp)((char *)&pools[2] - 2 * sizeof(void *)));

  printf("OK\n");
  return 0;
}

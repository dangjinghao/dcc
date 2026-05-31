#include "test.h"

char g_slop[1] = {0};
char g_ptr_target = 'X';

char g_arr[] = "123";

struct inner {
  long a;
  long b;
  char *p;
};

// Outer struct has a nested inner struct AND a bitfield.
// p should be initialized to g_slop, not to NULL.
struct outer {
  struct inner data;
  unsigned flag : 1;
};

static struct outer gobj = {{0, 0, g_slop}, 0};

// Second variant: pointer to a non-array global
struct inner_ptr {
  char *q;
  long x;
};

struct outer_ptr {
  struct inner_ptr data;
  unsigned flag : 1;
};

static struct outer_ptr gobj2 = {{&g_ptr_target, 42}, 0};

// Third variant: bitfield appears BEFORE the nested struct
struct outer_bf_first {
  unsigned flag : 1;
  struct inner data;
};

static struct outer_bf_first gobj3 = {0, {0, 0, g_slop}};

// Fourth variant: two nested structs with bitfield between them
struct inner_short {
  char *s;
  short pad;
};

struct outer_two {
  struct inner_short a;
  unsigned flag : 1;
  struct inner_short b;
};

static struct outer_two gobj4 = {{g_slop, 0}, 0, {&g_arr[1], 0}};

int main() {
  // Test 1: nested struct with pointer init + bitfield
  ASSERT(0, (int)gobj.data.a);
  ASSERT(0, (int)gobj.data.b);
  ASSERT(1, gobj.data.p == g_slop);
  ASSERT(0, gobj.flag);

  // Test 2: pointer to non-array global + bitfield
  ASSERT(1, gobj2.data.q == &g_ptr_target);
  ASSERT(*(gobj2.data.q), 'X');
  ASSERT(42, (int)gobj2.data.x);
  ASSERT(0, gobj2.flag);

  // Test 3: bitfield before nested struct
  ASSERT(0, gobj3.flag);
  ASSERT(1, gobj3.data.p == g_slop);

  // Test 4: two nested structs separated by bitfield
  ASSERT(1, gobj4.a.s == g_slop);
  ASSERT(0, gobj4.flag);
  ASSERT(1, gobj4.b.s == &g_arr[1]);
  ASSERT(*(gobj4.b.s), '2');

  printf("OK\n");
  return 0;
}

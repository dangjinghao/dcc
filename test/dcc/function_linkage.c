#include "test.h"

// C11 6.2.2p5: function declaration without storage-class specifier
// is treated "as if declared with extern", so it inherits the prior
// static declaration's internal linkage (6.2.2p4).

// Test 1: static decl, then definition without static
static int f1(void);
int f1(void) { return 1; }

// Test 2: static decl, then forward decl without static, then definition
static int f2(void);
int f2(void);
int f2(void) { return 2; }

// Test 3: static decl, then extern decl (traditional compat)
static int f3(void);
extern int f3(void);
int f3(void) { return 3; }

// Test 4: static decl, then inline definition
static int f4(void);
inline int f4(void) { return 4; }

// Test 5: static definition, then extern declaration
static int f5(void) { return 5; }
extern int f5(void);

// Test 6: no-static first, then static — should keep working
extern int f6(void);
extern int f6(void);
int f6(void) { return 6; }

int main() {
  ASSERT(1, f1());
  ASSERT(2, f2());
  ASSERT(3, f3());
  ASSERT(4, f4());
  ASSERT(5, f5());
  ASSERT(6, f6());

  printf("OK\n");
  return 0;
}

#include "test.h"

int main() {
  // Test 1: Basic label address retrieval
  // Labels can be converted to pointers
  void *addr1 = &&label1;
label1:
  ASSERT(1, addr1 != 0);
  void *addr2 = &&label2;
label2:
  // Test 2: Different labels have different addresses
  ASSERT(1, addr1 != addr2);

  // Test 3: Same label always has same address
  void *addr1_again = &&label1;
  ASSERT(1, addr1 == addr1_again);

  // Test 4: Indirect goto with label address
  int result = 0;
  void *target = &&success_label;
  goto *target;
  // Should not reach here
  ASSERT(1, 0);
success_label:
  result = 42;
  ASSERT(42, result);

  // Test 5: Multiple labels - array of addresses
  void *labels[3] = {&&label_a, &&label_b, &&label_c};
  int step = 0;

  // Jump to second label (index 1)
  goto *labels[1];

label_a:
  step = 1;
  goto end_test5;

label_b:
  step = 2;
  goto end_test5;

label_c:
  step = 3;

end_test5:
  ASSERT(2, step);

  // Test 6: Indirect goto in loop
  int counter = 0;
  void *handlers[2] = {&&handler0, &&handler1};

  // Call handler 0
  void *current_handler = handlers[0];
  goto *current_handler;

handler0:
  counter = 10;
  goto after_loop;

handler1:
  counter = 20;

after_loop:
  ASSERT(10, counter);

  // Test 7: Label address computation
  // This tests that we can use the indirect branch mechanism
  int choice = 1;
  void *jump_table[2] = {&&case0, &&case1};

  if (choice >= 0 && choice < 2) {
    goto *jump_table[choice];
  }

case0:
  counter = 100;
  goto after_switch;

case1:
  counter = 200;

after_switch:
  ASSERT(200, counter);

  printf("OK\n");
}

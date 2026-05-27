#include "test.h"
#include <stdatomic.h>
#include <threads.h>
void basic_atomic() {
  atomic_int a = 10;
  a += 5;
  ASSERT(15, a);

  a -= 3;
  ASSERT(12, a);

  a *= 2;
  ASSERT(24, a);

  a /= 4;
  ASSERT(6, a);

  a = 0xFF;
  a &= 0x0F;
  ASSERT(0x0F, a);

  a |= 0xF0;
  ASSERT(0xFF, a);

  a ^= 0x0F;
  ASSERT(0xF0, a);

  atomic_uint b = 1;
  b <<= 3;
  ASSERT(8, b);

  b >>= 2;
  ASSERT(2, b);

  atomic_long c = 100;
  c += 50;
  ASSERT(150, c);
  c -= 80;
  ASSERT(70, c);
  c *= 3;
  ASSERT(210, c);

  a = 7;
  int r = (a += 3);
  ASSERT(10, r);
  ASSERT(10, a);

  r = (a -= 5);
  ASSERT(5, r);
  ASSERT(5, a);

  r = (a *= 4);
  ASSERT(20, r);
  ASSERT(20, a);

  r = (a /= 2);
  ASSERT(10, r);
  ASSERT(10, a);

  a = 0xAA;
  r = (a &= 0x55);
  ASSERT(0, r);
  ASSERT(0, a);

  a = 0xAA;
  r = (a |= 0x55);
  ASSERT(0xFF, r);
  ASSERT(0xFF, a);

  a = 0xFF;
  r = (a ^= 0x0F);
  ASSERT(0xF0, r);
  ASSERT(0xF0, a);

  b = 1;
  unsigned s = (b <<= 5);
  ASSERT(32, s);
  ASSERT(32, b);

  s = (b >>= 3);
  ASSERT(4, s);
  ASSERT(4, b);
}

atomic_int shared_counter;
atomic_long shared_long_counter;

int thread_mul_div_task(void *arg) {
  for (int i = 0; i < 10; i++) {
    shared_counter *= 2;
    shared_counter /= 2;
  }
  return 0;
}

int thread_add_sub_task(void *arg) {
  for (int i = 0; i < 10; i++) {
    shared_counter += 1;
    shared_counter -= 1;
  }
  return 0;
}

int thread_long_task(void *arg) {
  for (int i = 0; i < 5; i++) {
    shared_long_counter *= 2;
    shared_long_counter /= 2;
  }
  return 0;
}

void concurrent_atomic_mul_div() {
  shared_counter = 100;

  thrd_t t1, t2, t3, t4;

  thrd_create(&t1, thread_mul_div_task, NULL);
  thrd_create(&t2, thread_mul_div_task, NULL);
  thrd_create(&t3, thread_add_sub_task, NULL);
  thrd_create(&t4, thread_add_sub_task, NULL);

  thrd_join(t1, NULL);
  thrd_join(t2, NULL);
  thrd_join(t3, NULL);
  thrd_join(t4, NULL);

  ASSERT(100, shared_counter);
}

void concurrent_atomic_long() {
  shared_long_counter = 1000;

  thrd_t t1, t2, t3;

  thrd_create(&t1, thread_long_task, NULL);
  thrd_create(&t2, thread_long_task, NULL);
  thrd_create(&t3, thread_long_task, NULL);

  thrd_join(t1, NULL);
  thrd_join(t2, NULL);
  thrd_join(t3, NULL);

  ASSERT(1000, shared_long_counter);
}

void float_atomic() {
  typedef _Atomic float atomic_float;
  typedef _Atomic double atomic_double;

  atomic_float a = 10.5f;
  a += 3.2f;
  ASSERT(1, a == 13.7f);
  a -= 2.5f;
  ASSERT(1, a == 11.2f);
  a *= 2.0f;
  ASSERT(1, a == 22.4f);
  a /= 4.0f;
  ASSERT(1, a == 5.6f);
  a = -5.0f;
  a += 10.0f;
  ASSERT(1, a == 5.0f);
  a *= -1.0f;
  ASSERT(1, a == -5.0f);
  atomic_double b = 20.5;
  b += 10.5;
  ASSERT(1, b == 31.0);
  b -= 15.0;
  ASSERT(1, b == 16.0);
  b *= 3.0;
  ASSERT(1, b == 48.0);
  b /= 4.0;
  ASSERT(1, b == 12.0);
  b = 1.0;
  b /= 3.0;
  ASSERT(1, b == 0.3333333333333333);
  b *= 3.0;
  ASSERT(1, b == 1.0);
  a = 7.5f;
  float r = (a += 2.5f);
  ASSERT(1, r == 10.0f);
  ASSERT(1, a == 10.0f);
  r = (a -= 3.0f);
  ASSERT(1, r == 7.0f);
  ASSERT(1, a == 7.0f);
  r = (a *= 2.0f);
  ASSERT(1, r == 14.0f);
  ASSERT(1, a == 14.0f);
  r = (a /= 7.0f);
  ASSERT(1, r == 2.0f);
  ASSERT(1, a == 2.0f);
  b = 100.0;
  double s = (b += 50.5);
  ASSERT(1, s == 150.5);
  ASSERT(1, b == 150.5);
  s = (b -= 100.5);
  ASSERT(1, s == 50.0);
  ASSERT(1, b == 50.0);
  s = (b *= 0.5);
  ASSERT(1, s == 25.0);
  ASSERT(1, b == 25.0);
  s = (b /= 0.5);
  ASSERT(1, s == 50.0);
  ASSERT(1, b == 50.0);
}

_Atomic(long *) shared_long_counter_ptr;

int thread_long_task_ptr1(void *arg) {
  for (int i = 0; i < 1000; i++) {
    atomic_fetch_add(&shared_long_counter_ptr, 2);
  }
  return 0;
}

int thread_long_task_ptr2(void *arg) {
  for (int i = 0; i < 1000; i++) {
    atomic_fetch_sub(&shared_long_counter_ptr, 1);
  }
  return 0;
}

void ptr_atomic() {
  shared_long_counter_ptr = NULL;
  thrd_t t1, t2, t3;
  thrd_t t5, t6;

  thrd_create(&t1, thread_long_task_ptr1, NULL);
  thrd_create(&t2, thread_long_task_ptr2, NULL);
  thrd_create(&t3, thread_long_task_ptr1, NULL);
  thrd_create(&t5, thread_long_task_ptr2, NULL);
  thrd_create(&t6, thread_long_task_ptr1, NULL);

  thrd_join(t1, NULL);
  thrd_join(t2, NULL);
  thrd_join(t3, NULL);

  thrd_join(t5, NULL);
  thrd_join(t6, NULL);
  _Atomic(long *) start = NULL;
  // It's not allowed to use `shared_long_counter_ptr - (_Atomic(long*))NULL`...
  ASSERT(4000, shared_long_counter_ptr - start);
}

int main() {
  basic_atomic();
  concurrent_atomic_mul_div();
  concurrent_atomic_long();
  float_atomic();
  ptr_atomic();
  printf("OK\n");
  return 0;
}

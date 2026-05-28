#include "test.h"

// DCC layouts bitfield structs as { [ty->size x i8] } to handle overlapping
// storage units from the flat-bit-counter layout. These tests verify the
// byte-level data matches GCC/Clang.

// Test 1: mixed char and long bitfields - verifies type-change packing
struct BF1 {
  char a : 3;
  char b : 7;
  long c : 15;
} ss1 = {.a = 1, .b = 2, .c = 3};

// Test 2: char bitfields, separate units - verifies same-type overflow
struct BF2 {
  char a : 3;
  char b : 6;
  char c;
  long d : 3;
} ss2 = {.a = 1, .b = 20, .c = 'X', .d = 2};

// Test 3: same-type long bitfields in one unit - verifies packing
struct BF3 {
  long a : 4;
  long b : 4;
  long c : 4;
} ss3 = {.a = 1, .b = 2, .c = 3};

// Test 4: empty initializer - verifies zero-initialization
struct BF4 {
  char a : 3;
  int b : 20;
  char c;
} ss4 = {};

int main() {
  unsigned char *p;

  // Test 1
  p = (unsigned char *)&ss1;
  ASSERT(1, p[0]);    // a=1 at byte 0 bits 0-2
  ASSERT(0x82, p[1]); // a+b+c overlap: byte1=0x82 (GCC confirmed)
  ASSERT(1, p[2]);    // c=3 spills to byte2
  ASSERT(0, p[3]);    // zero padding
  ASSERT(8, (int)sizeof(ss1));

  // Test 2
  p = (unsigned char *)&ss2;
  ASSERT(1, p[0]);   // a=1 at byte 0
  ASSERT(20, p[1]);  // b=20 at byte 1 (new char unit, offset 1)
  ASSERT('X', p[2]); // c='X'
  ASSERT(2, p[3]);   // d=2 at byte 3 (long unit starts byte 0, bit 24 → byte3)
  ASSERT(8, (int)sizeof(ss2));

  // Test 3
  p = (unsigned char *)&ss3;
  // long a:4=1 at bit0, b:4=2 at bit4, c:4=3 at bit8
  ASSERT(0x21, p[0]); // 1 | (2 << 4) = 1 | 32 = 0x21
  ASSERT(0x03, p[1]); // 3 at byte 1 bits 0-1 (bit8-9)
  ASSERT(8, (int)sizeof(ss3));

  // Test 4 - zero
  p = (unsigned char *)&ss4;
  ASSERT(0, p[0]);
  ASSERT(0, p[1]);
  ASSERT(0, p[2]);
  ASSERT(0, p[3]);

  printf("OK\n");
  return 0;
}

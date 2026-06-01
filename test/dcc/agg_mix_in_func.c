#include "test.h"

// From: cpython/Modules/_ctypes/_ctypes_test.c
// Test: aggregate (struct) values passed and returned by value in
// function calls, mixed with aggregate pointers and scalar arguments.

typedef struct {
  long left;
  long top;
  long right;
  long bottom;
} RECT;

typedef struct {
  long x;
  long y;
} POINT;

long left = 10;
long top = 20;
long right = 30;
long bottom = 40;

RECT ReturnRect(int i, RECT ar, RECT *br, POINT cp, RECT dr, RECT *er, POINT fp,
                RECT gr) {

  if (ar.left + br->left + dr.left + er->left + gr.left != left * 5) {
    ar.left = 100;
    return ar;
  }
  if (ar.right + br->right + dr.right + er->right + gr.right != right * 5) {
    ar.right = 100;
    return ar;
  }
  if (cp.x != fp.x) {
    ar.left = -100;
  }
  if (cp.y != fp.y) {
    ar.left = -200;
  }
  switch (i) {
  case 0:
    return ar;
    break;
  case 1:
    return dr;
    break;
  case 2:
    return gr;
    break;
  }
  return ar;
}

int main() {
  RECT r1 = {left, top, right, bottom};
  RECT r2 = {left, top, right, bottom};
  RECT r3 = {left, top, right, bottom};
  RECT r4 = {left, top, right, bottom};
  RECT r5 = {left, top, right, bottom};
  POINT p1 = {100, 200};
  POINT p2 = {100, 200};

  // Case 0: return ar
  RECT res0 = ReturnRect(0, r1, &r2, p1, r3, &r4, p2, r5);
  ASSERT(10, (int)res0.left);
  ASSERT(20, (int)res0.top);
  ASSERT(30, (int)res0.right);
  ASSERT(40, (int)res0.bottom);

  // Case 1: return dr
  RECT res1 = ReturnRect(1, r1, &r2, p1, r3, &r4, p2, r5);
  ASSERT(10, (int)res1.left);
  ASSERT(20, (int)res1.top);
  ASSERT(30, (int)res1.right);
  ASSERT(40, (int)res1.bottom);

  // Case 2: return gr
  RECT res2 = ReturnRect(2, r1, &r2, p1, r3, &r4, p2, r5);
  ASSERT(10, (int)res2.left);
  ASSERT(20, (int)res2.top);
  ASSERT(30, (int)res2.right);
  ASSERT(40, (int)res2.bottom);

  // Mismatched POINT: cp.x != fp.x
  POINT p3 = {300, 200};
  RECT res3 = ReturnRect(0, r1, &r2, p1, r3, &r4, p3, r5);
  ASSERT(-100, (int)res3.left);

  // Mismatched POINT: cp.y != fp.y
  POINT p4 = {100, 400};
  RECT res4 = ReturnRect(0, r1, &r2, p1, r3, &r4, p4, r5);
  ASSERT(-200, (int)res4.left);

  // Mismatched left sum: tweak one value
  RECT bad = {left + 1, top, right, bottom};
  RECT res5 = ReturnRect(0, bad, &r2, p1, r3, &r4, p2, r5);
  ASSERT(100, (int)res5.left);

  // Mismatched right sum
  RECT bad2 = {left, top, right + 1, bottom};
  RECT res6 = ReturnRect(0, bad2, &r2, p1, r3, &r4, p2, r5);
  ASSERT(100, (int)res6.right);

  printf("OK\n");
  return 0;
}

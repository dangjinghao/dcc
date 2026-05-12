int v = 1;

extern int printf(const char *, ...);
int main() {
  int result = __builtin_atomic_exchange(&v, 3);
  printf("%d\n", result);

  int oldval = 4;
  _Bool result2 = __builtin_compare_and_swap(&v, &oldval, 10);
  printf("%d, oldval: %d\n", result2, oldval); // 0 3
  result2 = __builtin_compare_and_swap(&v, &oldval, 10);
  printf("%d, oldval: %d\n", result2, oldval); // 1 3
  printf("val: %d\n", v);                      // 10
}


int main() {
  extern int printf(char *, ...);
  extern int A;
  printf("%d\n", A);
}

int A = 1;
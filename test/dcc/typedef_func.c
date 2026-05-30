extern int printf(char *, ...);
typedef int func_t(char *);

func_t F;

func_t *F2;

int F(char *s) {
  printf("%s\n", s);
  return 0;
}

int main() {
  F2 = F;
  F2("hello world");
}
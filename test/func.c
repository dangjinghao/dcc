extern int printf(const char *, ...);

static char *si = "123";
static int i = 1;
struct {
  char c;
  int i;
} s = {.i = 2};

int F3();

int F1(int i, float f, double d, void *p) {
  return 1.f;
  return -i;
  return *si;
  return 0;
  return i;
  return ({
    i;
    si;
    s.i;
  });
  //   return F3();
}

void *F2() {
  return &i;
  return &s.i;
  return (s.c, (void *)0);
  return __FUNCTION__;
}

int F3() { return 3; }
// int main(int argc, char *argv[]) { return calc(argc); }
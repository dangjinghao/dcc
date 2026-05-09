enum { X = 10 };
int a = 1 + X;
int b = -1 + 10 * 54 / 3;

double c = .2;
float d = .3 + 1.f;

int e[] = {1, 2, 3};
float f[] = {1., .52, 3.1f};

long g[10][3] = {{1, 2, 3}, [2] = {1, 2, 3}, [3][1] = 4};
char i[] = "123";
char *h = "hhh";
char *ip = &i[1];

extern int SUM;

struct SA {
  int i;
  char c;
} SAs;

struct SA SAs2;

struct {
  int i1;
  int i2;
  struct {
    char c;
    short s;
  } s;
} SB2 = {.i1 = 1, .i2 = 2, .s.s = 1};

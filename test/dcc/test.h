#define ASSERT(x, y) assert(x, y, __FILE_NAME__, __LINE__, #y)

#define alloca(x) __builtin_alloca(x)

int printf(char *fmt, ...);
int sprintf(char *buf, char *fmt, ...);
int vsprintf(char *buf, char *fmt, void *ap);
int strcmp(char *p, char *q);
int strncmp(char *p, char *q, long n);
int memcmp(char *p, char *q, long n);
void exit(int n);
int vsprintf();
long strlen(char *s);
void *memcpy(void *dest, void *src, long n);
void *memset(void *s, int c, long n);
static inline void assert(int expected, int actual, char *filename, int line, char *code) {
  if (expected != actual) {
    printf("%s:%d, %s => %d expected but got %d\n", filename, line, code,
           expected, actual);
    exit(1);
  }
}
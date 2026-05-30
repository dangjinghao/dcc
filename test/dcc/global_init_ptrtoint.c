typedef long int intptr_t;

intptr_t x = (intptr_t)"hello world\n";
int main() {
  extern int printf(char *, ...);
  printf("%s", (char *)x);
}
struct small_struct {
  int a;
  char c;
};

struct large_struct {
  long a;
  char b;
  long c;
};

// struct small_struct small_struct(){
//     return (struct small_struct){};
// }

// struct large_struct large_struct(){
//     return (struct large_struct){};
// }

int small_struct(struct small_struct s){
    return s.a == 0;
}

int large_struct(struct large_struct s) { return s.a == 0; }

extern int printf(const char *, ...);
int main() {
  struct small_struct s = {.a = 0};
  printf("%d\n", small_struct(s));
  s.a = 1;
  printf("%d\n", small_struct(s));

  struct large_struct ls = {.a = 0};
  printf("%d\n", large_struct(ls));
  ls.a = 1;
  printf("%d\n", large_struct(ls));
}
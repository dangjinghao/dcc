struct small_struct {
  void *a;
  void *b;
};

struct large_struct {
  void *a;
  void *b;
  void *c;
};

// struct small_struct small_struct(){
//     return (struct small_struct){};
// }

// struct large_struct large_struct(){
//     return (struct large_struct){};
// }

// int small_struct(struct small_struct s){
//     return s.a == 0;
// }

int large_struct(struct large_struct s) { return s.a == 0; }

extern int printf(const char *, ...);
int main() {
  struct large_struct s = {.a = (void *)0};
  printf("%d\n", large_struct(s));
  s.a = (void*)1;
  printf("%d\n", large_struct(s));

}
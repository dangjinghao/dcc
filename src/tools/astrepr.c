#include "dcc.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    exit(1);
  }
  Token *ts = tokenize_file(argv[1]);

  Obj *o = parse(ts);
  char *s = objrepr(o);
  puts(s);
  return 0;
}

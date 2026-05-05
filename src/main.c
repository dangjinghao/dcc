#include "dcc.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    exit(1);
  }
  Token *ts = tokenize_file(argv[1]);

  Obj *ast = parse(ts);
  // Open a temporary output buffer.
  char *buf;
  size_t buflen;
  FILE *output_buf = open_memstream(&buf, &buflen);

  codegen(ast, output_buf);
  fclose(output_buf);
  puts(buf);
  return 0;
}

#include "dcc.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>

noreturn void cc1() {
  Token *tokens = tokenize_file(opt_cc1_input);
  Obj *ast = parse(tokens);
  FILE *output_file = NULL;
  if (strcmp(opt_cc1_output, "-") == 0) {
    output_file = stdout;
  } else {
    output_file = fopen(opt_cc1_output, "w");
  }
  codegen(ast, output_file);
  if (output_file != stdout)
    fclose(output_file);
  exit(0);
}

int main(int argc, char *argv[]) {
  parse_args(argc, argv);
  if (opt_cc1) {
    cc1();
  }

  exit(1);
}

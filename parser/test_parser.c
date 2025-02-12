#include "convert/convert.h"
#include "lexer.h"
#include "parser.h"
int str_parse_main() {
  struct lexer lexer;
  lexer_from_string(&lexer, "a1,a2,V1 = V2 = C1?C2?T1:F1:C3?T2:F2");
  struct parser parser;
  parser_from_lexer(&parser, &lexer);
  astn n = parse_expr(&parser);
  sds buf = sdsempty();
  buf = convert_expr_obj_to_repr(n, buf);
  printf("%s\n", buf);
  sdsfree(buf);
  lexer_destroy(&lexer);

  return 0;
}

int main() { return str_parse_main(); }
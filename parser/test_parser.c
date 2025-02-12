#include "convert/convert.h"
#include "lexer.h"
#include "parser.h"
int str_parse_main() {
  struct lexer lexer;
  lexer_from_string(&lexer, "F1,F2,F3,F4,+1+--B*A++-C*D/E-6");
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
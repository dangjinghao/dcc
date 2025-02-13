#include "convert/convert.h"
#include "lexer.h"
#include "parser.h"

int process(struct lexer *lexer) {
  struct parser parser;
  parser_from_lexer(&parser, lexer);
  astn n = parse_comma_expr(&parser);
  sds buf = sdsempty();
  buf = convert_expr_obj_to_repr(n, buf);
  parser_free_ast(n);
  printf("%s\n", buf);
  sdsfree(buf);
  return 0;
}

int str_parse() {
  struct lexer lexer;
  lexer_from_string(&lexer, "a1,a2,V1 = V2 = C1?C2?T1:F1:C3?T2:F2");
  process(&lexer);
  lexer_destroy(&lexer);
  return 0;
}

int str_paren_parse() {
  struct lexer lexer;
  lexer_from_string(&lexer, "1+(((((1))))) + 2*(3+4)");
  process(&lexer);
  lexer_destroy(&lexer);

  return 0;
}

int str_combine(){
  struct lexer lexer;
  lexer_from_string(&lexer, ".1f + \"123\" \"456\" + '\\'' ");
  process(&lexer);
  lexer_destroy(&lexer);

  return 0;
}

int main() { return str_combine(); }
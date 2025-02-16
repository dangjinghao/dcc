#include "ast.h"
#include "convert/convert.h"
#include "lexer.h"
#include "log/log.h"
#include "parser.h"
#include <stdbool.h>

int process_expr(struct lexer *lexer) {
  struct parser parser;
  parser_from_lexer(&parser, lexer);
  astn n = parse_expression(&parser);
  sds buf = sdsempty();
  buf = convert_ast_to_repr(n, buf);
  ast_free(n);
  printf("%s\n", buf);
  sdsfree(buf);
  parser_destory(&parser);

  return 0;
}

int process_declaration(struct lexer *lexer) {
  struct parser parser;
  parser_from_lexer(&parser, lexer);
  astn n = ast_new(ast_block);
  parse_external_declaration(&parser, n);
  sds buf = sdsempty();
  buf = convert_ast_to_repr(n, buf);
  ast_free(n);
  printf("%s\n", buf);
  sdsfree(buf);
  parser_destory(&parser);
  return 0;
}

int process_trans_unit(struct lexer *lexer) {
  struct parser parser;
  parser_from_lexer(&parser, lexer);
  astn n = parse_translation_unit(&parser);
  sds buf = sdsempty();
  buf = convert_ast_to_repr(n, buf);
  ast_free(n);
  printf("%s\n", buf);
  sdsfree(buf);
  parser_destory(&parser);
  return 0;
}

int process_statement(struct lexer *lexer) {
  struct parser parser;
  parser_from_lexer(&parser, lexer);
  astn n = parse_statement(&parser);
  sds buf = sdsempty();
  buf = convert_ast_to_repr(n, buf);
  ast_free(n);
  printf("%s\n", buf);
  sdsfree(buf);
  parser_destory(&parser);
  return 0;
}

int str_parse() {
  struct lexer lexer;
  lexer_from_string(&lexer, "a1,a2,V1 = V2 = C1?C2?T1:F1:C3?T2:F2");
  process_expr(&lexer);
  lexer_destroy(&lexer);
  return 0;
}

int str_paren_parse() {
  struct lexer lexer;
  lexer_from_string(&lexer, "1+(((((1))))) + 2*(3+4)");
  process_expr(&lexer);
  lexer_destroy(&lexer);

  return 0;
}

int str_combine() {
  struct lexer lexer;
  lexer_from_string(&lexer, ".1f + \"123\" \"456\" + '\\'' ");
  process_expr(&lexer);
  lexer_destroy(&lexer);

  return 0;
}

int declaration() {
  struct lexer lexer;
  lexer_from_string(&lexer,
                    "int a = 1, b = a, **((*c)) = 1+(((((1))))) + 2*(3+4);");
  process_trans_unit(&lexer);
  lexer_destroy(&lexer);
  return 0;
}

int statement() {
  struct lexer lexer;
  lexer_from_string(&lexer, "a = b = 1;");
  process_statement(&lexer);
  lexer_destroy(&lexer);
  return 0;
}

int main() {
  log_color_enable(true);
  return statement();
}
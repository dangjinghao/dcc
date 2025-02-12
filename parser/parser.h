#ifndef PARSER_H
#define PARSER_H
#include "ast.h"
#include "lexer.h"
struct parser {
  struct lexer *lexer;
  int current_token;
  struct ast_node *root;
};
void parser_from_lexer(struct parser *parser, struct lexer *lexer);
int parser_consume(struct parser *parser);

astn parse_expr(struct parser *parser);
astn __parse_expr(struct parser *parser, int ctx_prec);
#endif
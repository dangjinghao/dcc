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
int parser_consume_with(struct parser *parser, int token);
void parser_free_ast(astn node);
astn parse_expr(struct parser *parser);
astn __parse_expr(struct parser *parser, int ctx_prec);
#endif
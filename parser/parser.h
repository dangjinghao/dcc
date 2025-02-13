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
void parser_snapshot(struct parser *_new, struct parser *_old);
int parser_consume(struct parser *parser);
int parser_consume_with(struct parser *parser, int token);
void parser_free_ast(astn node);
astn parse_assign_expr(struct parser *parser);
astn __parse_assign_expr(struct parser *parser, int ctx_prec);
astn parse_comma_expr(struct parser *parser);

#endif
#ifndef PARSER_H
#define PARSER_H
#include "lexer.h"
struct parser {
  struct lexer *lexer;
  int current_token;
  struct ast_node *root;
};
void parser_eat(struct parser *parser, int tok);
int parser_consume(struct parser *parser);
#endif
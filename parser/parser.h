#ifndef PARSER_H
#define PARSER_H
#include "ast.h"
#include "dynarray/dynarray.h"
#include "lexer.h"
typedef struct parser {
  struct lexer *lexer;
  int current_token;
  // add NULL if we meet a new scope
  struct dynarray idtab;
  struct dynarray tagtab; //enum, struct, union
} *parser;
void parser_from_lexer(parser parser, struct lexer *lexer);
void parser_snapshot(parser _new, parser _old);
void parser_free(parser parser);
int parser_consume(parser parser);
int parser_consume_with(parser parser, int token);
void parser_free_ast(astn node);
astn parse_assign_expr(parser parser);
astn __parse_assign_expr(parser parser, int ctx_prec);
astn parse_comma_expr(parser parser);
astn parse_ident(parser parser);
dynarray parse_declarator(parser parser, dynarray type_chain);
bool parser_check_constant_expr(astn expr);
astn parse_constant_expr(parser parser);
astn parse_external_declaration(parser parser, astn block);

#endif
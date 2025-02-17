#ifndef PARSER_H
#define PARSER_H
#include "ast.h"
#include "dynarray/dynarray.h"
#include "lexer.h"
typedef struct parser {
  struct lexer *lexer;
  int current_token;
  // add NULL if we meet a new scope. store the *ref* of ast node
  struct dynarray idtab;
  struct dynarray tagtab; //enum, struct, union
} *parser;
void parser_from_lexer(parser parser, struct lexer *lexer);
void parser_snapshot(parser _new, parser _old);
void parser_destory(parser parser);
int parser_consume(parser parser);
int parser_consume_with(parser parser, int token);
astn parse_assign_expr(parser parser);
astn __parse_assign_expr(parser parser, int ctx_prec);
astn parse_expression(parser parser);
astn parse_ident(parser parser);
dynarray parse_declarator(parser parser, dynarray type_chain);
bool parser_check_constant_expr(astn expr);
astn parse_constant_expr(parser parser);
astn parse_external_declaration(parser parser, astn block);
astn parse_labeled_statement(parser p);
astn parse_statement(parser p);
astn parse_translation_unit(parser parser);
astn parse_compound_statement(parser parser);
void parser_add_declaration_to_current_scope_table(astn decl, dynarray tab);
astn parser_find_declaration_in_current_scope_table(sds ident, dynarray tab);
astn parser_find_declaration_in_all_scope_table(sds ident, dynarray tab);

void parser_pop_scope(parser parser);
void parser_push_scope(parser parser);
astn parser_get_typedef(parser parser, sds ident);
#endif
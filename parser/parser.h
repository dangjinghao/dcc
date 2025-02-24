#ifndef PARSER_H
#define PARSER_H
#include "ast.h"
#include "lexer.h"
#include "slist/slist.h"
#include <stdbool.h>
typedef struct parser {
  struct lexer *lexer;
  int current_token;
  // add NULL if we meet a new scope. store the *ref* of ast node
  struct slist idtab;
  // enum, struct, union
  struct slist tagtab;
  // for strin or static in function which would be promoted to global
  astn global_block;
  // for break, continue, case and default
  astn interruptable_block;
  // for return statement
  astn current_function_block;
  // for unique id declaration
  size_t global_uid, local_uid;
} *parser;
void parser_from_lexer(parser parser, struct lexer *lexer);
void parser_snapshot(parser _new, parser _old);
void parser_destory(parser parser);
void parser_restore(parser target, parser snapshot);
int parser_consume(parser parser);
int parser_consume_with(parser parser, int token);
void parser_add_declaration_to_current_scope_table(astn decl, slist tab);
astn parser_find_declaration_in_current_scope_table(sds ident, slist tab);
astn parser_find_declaration_in_all_scope_table(sds ident, slist tab);
void parser_pop_scope(parser parser);
void parser_push_scope(parser parser);
astn parser_get_typedef_by_type_name(parser parser, sds ident);
bool parser_is_current_block_global(parser parser);
void parser_declare_new_tag(parser parser, astn n);
void parser_declare_new_symbol(parser parser, astn n);
bool parser_check_constant_expr(astn expr);
size_t parser_get_local_uid(parser parser);
size_t parser_get_global_uid(parser parser);
void parser_set_declaration_uid(astn n, parser parser);

sds parse_remove_type_chain_ident(slist type_chain);
astn parse_unary(parser parser);
astn parse_specifier_qualifiers(parser parser);
astn parse_assign_expr(parser parser);
astn __parse_assign_expr(parser parser, int ctx_prec);
astn parse_expression(parser parser);
astn parse_ident(parser parser);
astn parse_init_declarator(parser parser, astn decl_specs,
                           bool delay_alloc_uid);
slist parse_declarator(parser parser, slist type_chain);
astn parse_constant_expr(parser parser);
void parse_external_declaration(parser parser, slist block);
astn parse_labeled_statement(parser p);
astn parse_statement(parser p);
astn parse_translation_unit(parser parser);
astn parse_compound_statement(parser parser);
slist parse_type_name(parser parser, slist type_chain);
astn parse_initializer(parser parser);
#endif
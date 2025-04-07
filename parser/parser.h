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
  struct slist symtab;
  // for break, iteration and switch statement will use this
  astn break_scope;
  // for continue, iteration statement will use this
  astn continue_scope;
  // for switch statement
  astn switch_scope;
  // do not modify the value for some ugly depenencies
  astn function_scope;
  // for unique id declaration
  size_t uidcnt;
} *parser;

void parser_new_from_lexer(parser parser, struct lexer *lexer);
void parser_new_snapshot(parser _new, parser _old);
void parser_destory(parser parser);
void parser_restore(parser target, parser snapshot);
int parser_consume(parser parser);
int parser_consume_with(parser parser, int token);
void parser_scope_current_add_symbol(astn decl, slist tab);
astn parser_scope_current_find_ident(sds ident, slist tab);
astn parser_scope_all_find_ident(sds ident, slist tab);
void parser_pop_scope(parser parser);
void parser_push_scope(parser parser);
astn parser_lookup_typedef(parser parser, sds ident);
bool parser_scope_is_current_global(parser parser);
astn parser_new_tag(parser parser, astn n);
void parser_new_declaration(parser parser, astn n);
void parser_new_enumerator(parser parser, astn n);
bool parser_expr_check_const_int(astn expr);
size_t parser_get_uid(parser parser);
void parser_unfold_type_chain(parser parser, slist type_chain);
long parser_expr_eval_const_int(astn expr);
slist parser_gen_symtab(parser parser);

sds parse_type_chain_pop_ident(slist type_chain);
astn parse_expr_unary(parser parser);
astn parse_specifier_qualifiers(parser parser);
astn parse_expr_assign(parser parser);
astn parse_expr_assign_inner(parser parser, int ctx_prec);
astn parse_expression(parser parser);
astn parse_expr_ident(parser parser);
astn parse_init_declarator(parser parser, astn decl_specs,
                           bool delay_alloc_uid);
slist parse_declarator(parser parser, slist type_chain);
astn parse_expr_const_int(parser parser);
void parse_external_declaration(parser parser, slist block);
astn parse_statement_labeled(parser p);
astn parse_statement(parser p);
astn parse_translation_unit(parser parser);
astn parse_statement_compound(parser parser);
slist parse_type_name(parser parser, slist type_chain);
astn parse_initializer(parser parser);
sds parser_declaration_get_ident(astn n);

void parser_symtab_add(parser parser, astn n);
astn parser_symtab_find(slist symtab, sds id);
#endif
#ifndef GRAMMAR_H
#define GRAMMAR_H
#include "ast.h"
#include "lexer.h"
#include "macro/macro.h"
#include "parser.h"
#include "slist/slist.h"
#include <assert.h>

/**
 * @grammar
 * <type-qualifier> ::= const
 *                   | volatile
 * @param parser 
 */
static inline bool g_is_type_qualifier_firstset(parser parser) {
  int token = parser->current_token;
  return token == TOK_KW_CONST || token == TOK_KW_VOLATILE;
}

/**
 * @grammar
 * <storage-class-specifier> ::= auto
 *                            | register
 *                            | static
 *                            | extern
 *                            | typedef
 * @param parser
 */
static inline bool g_is_storage_class_specifier_firstset(parser parser) {
  static int reserved_kw[] = {TOK_KW_AUTO, TOK_KW_REGISTER, TOK_KW_STATIC,
                              TOK_KW_EXTERN, TOK_KW_TYPEDEF};
  int token = parser->current_token;
  return ARRAY_IN(reserved_kw, token, EQ_EQ);
}

static inline bool g_is_declaration_typedef(astn d) {
  assert(d->type == ast_declaration);
  astn t = slist_peek_tail(&d->declaration.type_chain);
  assert(t->type == ast_ctype);
  return t->ctype.storage == TOK_KW_TYPEDEF;
}

/**
 * @grammar
 * <typedef-name> ::= identifier
 * @param parser 
 */
static inline bool g_is_typedef_name_firstset(parser parser) {
  if (parser->current_token != TOK_IDENT) {
    return false;
  }
  return parser_get_typedef_by_type_name(
             parser, parser->lexer->lex_token._ident) != NULL;
}

/**
 * @grammar
 * <enum-specifier> ::= enum <identifier> { <enumerator-list> }
 *                    | enum { <enumerator-list> }
 *                    | enum <identifier>
 * @param parser 
 */
static inline bool g_is_enum_specifier(parser parser) {
  return parser->current_token == TOK_KW_ENUM;
}

/**
 * @grammar
 * <enumerator> ::= <identifier>
 *               | <identifier> = <constant-expression>
 * @param parser 
 */
static inline bool g_is_enumerator_firstset(parser parser) {
  return parser->current_token == TOK_IDENT;
}

/**
 * @grammar
 * <enumerator-list> ::= <enumerator>
 *                    | <enumerator-list> , <enumerator>
 * 
 */
static inline bool g_is_enumerator_list_firstset(parser parser) {
  return g_is_enumerator_firstset(parser);
}
/**
 * @grammar
 * <struct-or-union> ::= struct
 *                    | union
 * @param parser 
 */
static inline bool g_is_struct_or_union_firstset(parser parser) {
  int token = parser->current_token;
  return token == TOK_KW_STRUCT || token == TOK_KW_UNION;
}

/**
 * @grammar
 * <struct-or-union-specifier> ::= <struct-or-union> <identifier> { {<struct-declaration>}+ }
 *                               | <struct-or-union> { {<struct-declaration>}+ }
 *                               | <struct-or-union> <identifier>
 */
static inline bool g_is_struct_or_union_specifier(parser parser) {
  return g_is_struct_or_union_firstset(parser);
}

/**
 * @grammar
 * <type-specifier> ::= void
 *                   | char
 *                   | short
 *                   | int
 *                   | long
 *                   | float
 *                   | double
 *                   | signed
 *                   | unsigned
 *                   | <struct-or-union-specifier>
 *                   | <enum-specifier> 
 *                   | <typedef-name>
 * @param token 
 */
static inline bool g_is_type_specifier_firstset(parser parser) {
  int token = parser->current_token;
  static int reserved_kw[] = {TOK_KW_VOID,   TOK_KW_CHAR,   TOK_KW_SHORT,
                              TOK_KW_INT,    TOK_KW_LONG,   TOK_KW_FLOAT,
                              TOK_KW_DOUBLE, TOK_KW_SIGNED, TOK_KW_UNSIGNED};
  if (ARRAY_IN(reserved_kw, token, EQ_EQ))
    return true;
  else if (g_is_struct_or_union_specifier(parser))
    return true;
  else if (g_is_enum_specifier(parser))
    return true;
  else if (g_is_typedef_name_firstset(parser))
    return true;

  return false;
}

/**
 * @grammar
 * <declaration-specifier> ::= <storage-class-specifier>
 *                          | <type-specifier>
 *                          | <type-qualifier>
 */
static inline bool g_is_declaration_specifier_firstset(parser parser) {
  return g_is_storage_class_specifier_firstset(parser) ||
         g_is_type_specifier_firstset(parser) ||
         g_is_type_qualifier_firstset(parser);
}

/**
 * @grammar
 * <pointer> ::= * {<type-qualifier>}* {<pointer>}?
 * 
 */
static inline bool g_is_pointer_firstset(parser parser) {
  return parser->current_token == '*';
}

/**
 * @brief also support abstract direct-declarator
 * @grammar, left recursive.
 * <direct-declarator> ::= {<identifier>}?
 *                      | ( <declarator> )
 *                      | <direct-declarator> [ {<constant-expression>}? ]
 *                      | <direct-declarator> ( {<parameter-type-list>}? )
 *
 * @grammar, right recursive.
 * <direct-declarator> ::= {<identifier>}? <direct-declarator-suffix>
 *                      | ( <declarator> ) <direct-declarator-suffix>
 * 
 * <direct-declarator-suffix> ::= [ {<constant-expression>}? ] <direct-declarator-suffix>
 *                             | ( {<parameter-type-list>}? ) <direct-declarator-suffix>
 *                             | ε
 */
static inline bool g_is_direct_declarator_firstset(parser parser) {
  int token = parser->current_token;
  return token == TOK_IDENT || token == '(';
}

/**
 * @grammar
 * <declarator> ::= {<pointer>}? <direct-declarator>
 */
static inline bool g_is_declarator_firstset(parser parser) {
  if (g_is_pointer_firstset(parser)) {
    return true;
  }
  if (g_is_direct_declarator_firstset(parser)) {
    return true;
  }
  return false;
}

/**
 * @grammar 
 * <function-definition> ::= {<declaration-specifier>}+ <declarator> <compound-statement>
 * 
 */
static inline bool g_is_function_definition_firstset(parser parser) {
  return g_is_declaration_specifier_firstset(parser);
}

/**
 * @grammar
 * <declaration> ::=  {<declaration-specifier>}+ {{<init-declarator>} {, <init-declarator>}*}? ;
 */

static inline bool g_is_declaration_firstset(parser parser) {
  return g_is_declaration_specifier_firstset(parser);
}

/**
 * @grammar
 * <external-declaration> ::= <function-definition>
 *                         | <declaration>
 * @param parser 
 */
static inline bool g_is_external_declaration_firstset(parser parser) {
  return g_is_declaration_specifier_firstset(parser);
}

/**
 * @grammar 
 * <translation-unit> ::= {<external-declaration>}*
 */

static inline bool g_is_translation_unit_firstset(parser parser) {
  int token = parser->current_token;
  if (token == TOK_EOF)
    return true;
  else if (g_is_external_declaration_firstset(parser))
    return true;

  return false;
}

static inline bool g_is_sign_tok(enum tok_type tok) {
  return tok == TOK_KW_SIGNED || tok == TOK_KW_UNSIGNED;
}

static inline bool g_is_int_family_tok(enum tok_type tok) {
  static int reserved_kw[] = {TOK_KW_CHAR, TOK_KW_SHORT, TOK_KW_INT,
                              TOK_KW_LONG};
  return ARRAY_IN(reserved_kw, tok, EQ_EQ) || g_is_sign_tok(tok);
}

static inline bool g_is_fp_family_tok(enum tok_type tok) {
  return tok == TOK_KW_FLOAT || tok == TOK_KW_DOUBLE;
}

/**
 * @grammar
 * <init-declarator> ::= <declarator>
 *                    | <declarator> = <initializer>
 */
static inline bool g_is_init_declarator_firstset(parser parser) {
  return g_is_declarator_firstset(parser);
}

static inline bool g_is_primary_expression_firstset(parser parser) {
  return parser->current_token == TOK_IDENT || parser->current_token == '(' ||
         (parser->current_token > __TOK_LIT_START &&
          parser->current_token < TOK_SYM_LEQ);
}

/**
 * @grammar
 * <unary-expression> ::= <postfix-expression>
 *                     | ++ <unary-expression>
 *                     | -- <unary-expression>
 *                     | <unary-operator> <cast-expression>
 *                     | sizeof <unary-expression>
 *                     | sizeof <type-name>
 *
 * <unary-operator> ::= &
 *                   | *
 *                   | +
 *                   | -
 *                   | ~
 *                   | !

 * @param parser 
 * @return true 
 * @return false 
 */
static inline bool g_is_unary_expression_firstset(parser parser) {
  int prefix_uops[] = {
      TOK_SYM_SELF_INC,
      TOK_SYM_SELF_DEC,
      TOK_KW_SIZEOF,
      '-',
      '+',
      '!',
      '~',
      '*',
      '&',
  };
  return ARRAY_IN(prefix_uops, parser->current_token, EQ_EQ) ||
         g_is_primary_expression_firstset(parser);
}

/**
 * @brief 
 * <cast-expression> ::= <unary-expression>
 *                    | ( <type-name> ) <cast-expression>
 * 
 * @param parser 
 * @return true 
 * @return false 
 */
static inline bool g_is_cast_expression_firstset(parser parser) {
  return parser->current_token == '(' || g_is_unary_expression_firstset(parser);
}

static inline bool g_is_assignment_expression_firstset(parser parser) {
  return g_is_unary_expression_firstset(parser);
}

/**
 * @grammar
 * <initializer> ::= <assignment-expression>
 *                | { <initializer-list> }
 *                | { <initializer-list> , }
 * @param parser 
 * @return true 
 * @return false 
 */
static inline bool g_is_initializer_firstset(parser parser) {
  return g_is_assignment_expression_firstset(parser) ||
         parser->current_token == '{';
}

/**
 * @grammar
 * <initializer-list> ::= <initializer>
 *                     | <initializer-list> , <initializer>
 */
static inline bool g_is_initializer_list_firstset(parser parser) {
  return g_is_initializer_firstset(parser);
}

/**
 * @brief because of the implementation features, we have to modify the BNF grammar to 
 * make it support define function in compound statement.
 * @grammar
 * <compound-statement> ::= { {{<external-declaration>} | {<statement>}}* }
 * @param parser 
 * @return true 
 * @return false 
 */
static inline bool g_is_compound_statement_firstset(parser parser) {
  return parser->current_token == '{';
}

/**
 * @grammar
 * <labeled-statement> ::= <identifier> : <statement>
 *                      | case <constant-expression> : <statement>
 *                      | default : <statement>
 * 
 */
static inline bool g_is_labeled_statement_firstset(parser parser) {
  return parser->current_token == TOK_IDENT ||
         parser->current_token == TOK_KW_CASE ||
         parser->current_token == TOK_KW_DEFAULT;
}

/**
 * @grammar
 * <expression> ::= <assignment-expression>
 *               | <expression> , <assignment-expression>
 * @param parser 
 * @return true 
 * @return false 
 */
static inline bool g_is_expression_firstset(parser parser) {
  return g_is_assignment_expression_firstset(parser);
}

/**
 * @grammar
 * <expression-statement> ::= {<expression>}? ;
 * @param parser 
 * @return true 
 * @return false 
 */
static inline bool g_is_expression_statement_firstset(parser parser) {
  return g_is_expression_firstset(parser) || parser->current_token == ';';
}

/**
 * @grammar
 * <selection-statement> ::= if ( <expression> ) <statement>
 *                        | if ( <expression> ) <statement> else <statement>
 *                        | switch ( <expression> ) <statement>
 * @param parser 
 * @return true 
 * @return false 
 */
static inline bool g_is_selection_statement_firstset(parser parser) {
  return parser->current_token == TOK_KW_IF ||
         parser->current_token == TOK_KW_SWITCH;
}
/**
 * @grammar
 * <iteration-statement> ::= while ( <expression> ) <statement>
 *                         | do <statement> while ( <expression> ) ;
 *                         | for ( {<expression>}? ; {<expression>}? ; {<expression>}? ) <statement>
 */
static inline bool g_is_iteration_statement_firstset(parser parser) {
  return parser->current_token == TOK_KW_WHILE ||
         parser->current_token == TOK_KW_DO ||
         parser->current_token == TOK_KW_FOR;
}

/**
 * @grammar
 * <jump-statement> ::= goto <identifier> ;
 *                   | continue ;
 *                   | break ;
 *                   | return {<expression>}? ;
 */
static inline bool g_is_jump_statement_firstset(parser parser) {
  return parser->current_token == TOK_KW_GOTO ||
         parser->current_token == TOK_KW_CONTINUE ||
         parser->current_token == TOK_KW_BREAK ||
         parser->current_token == TOK_KW_RETURN;
}

/**
 * @grammar
 * <statement> ::= <labeled-statement>
 *              | <expression-statement>
 *              | <compound-statement>
 *              | <selection-statement>
 *              | <iteration-statement>
 *              | <jump-statement>
 * @param parser 
 * @return true 
 * @return false 
 */
static inline bool g_is_statement_firstset(parser parser) {
  return g_is_labeled_statement_firstset(parser) ||
         g_is_expression_statement_firstset(parser) ||
         g_is_compound_statement_firstset(parser) ||
         g_is_selection_statement_firstset(parser) ||
         g_is_iteration_statement_firstset(parser) ||
         g_is_jump_statement_firstset(parser);
}

/**
 * @grammar
 * <parameter-declaration> ::= {<declaration-specifier>}+ <declarator>
 *                           | {<declaration-specifier>}+ <abstract-declarator>
 *                           | {<declaration-specifier>}+
 * @param parser 
 * @return true 
 * @return false 
 */
static inline bool g_is_parameter_declaration_firstset(parser parser) {
  return g_is_declaration_specifier_firstset(parser);
}

/**
 * @grammar
 * <parameter-list> ::= <parameter-declaration>
 *                   | <parameter-list> , <parameter-declaration>
 * @param parser 
 * @return true 
 * @return false 
 */
static inline bool g_is_parameter_list_firstset(parser parser) {
  return g_is_parameter_declaration_firstset(parser);
}

/**
 * @grammar
 * <parameter-type-list> ::= <parameter-list>
 *                        | <parameter-list> , ...
 * @param parser 
 * @return true 
 * @return false 
 */
static inline bool g_is_parameter_type_list_firstset(parser parser) {
  return g_is_parameter_list_firstset(parser) || true;
}

static inline astn g_get_function_params(astn declaration) {
  assert(declaration->type == ast_declaration);
  astn ref = slist_peek_head(&declaration->declaration.type_chain);
  if (ref->type == ast_parameters) {
    return ref;
  }
  return NULL;
}

/**
 * @brief 
 * @grammar
 * <specifier-qualifier> ::= <type-specifier>
 *                        | <type-qualifier>
 */
static inline bool g_is_specifier_qualifier_firstset(parser parser) {
  return g_is_type_specifier_firstset(parser) ||
         g_is_type_qualifier_firstset(parser);
}

/**
 * @brief 
 * @grammar
 * <type-name> ::= {<specifier-qualifier>}+ {<abstract-declarator>}?
 * @param parser 
 * @return true 
 * @return false 
 */
static inline bool g_is_type_name_firstset(parser parser) {
  return g_is_specifier_qualifier_firstset(parser);
}

/**
 * @grammar
 * <struct-declarator> ::= <declarator>
 *                      | <declarator> : <constant-expression>
 *                      | : <constant-expression>
 * 
 */
static inline bool g_is_struct_declarator_firstset(parser parser) {
  return g_is_declarator_firstset(parser) || parser->current_token == ':';
}

/**
 * @grammar
 * <struct-declaration> ::= {<specifier-qualifier>}+ <struct-declarator-list> ;
 * 
 * @param parser 
 * @return true 
 * @return false 
 */
static inline bool g_is_struct_declaration_firstset(parser parser) {
  return g_is_specifier_qualifier_firstset(parser);
}

/**
 * @brief 
 * @grammar
 * <struct-declarator-list> ::= <struct-declarator>
 *                            | <struct-declarator-list> , <struct-declarator>
 * @param parser 
 * @return true 
 * @return false 
 */
static inline bool g_is_struct_declarator_list_firstset(parser parser) {
  return g_is_struct_declarator_firstset(parser);
}

static inline astn g_get_function_body(astn declaration) {
  if (declaration->declaration.extdata) {
    assert(declaration->declaration.extdata->type == ast_block);
    return declaration->declaration.extdata;
  }
  return NULL;
}

static inline bool g_is_function_definition(astn declaration) {
  return g_get_function_params(declaration) &&
         g_get_function_body(declaration) != NULL;
}

static inline bool g_is_empty_statement(astn statement) {
  return statement->type == ast_expr_primary &&
         statement->primary.type == TOK_EOF;
}

static inline astn g_new_empty_statement() {
  astn n = ast_new(ast_expr_primary);
  n->primary.type = TOK_EOF;
  return n;
}

static inline bool g_is_function_declaration(astn declaration) {
  return g_get_function_params(declaration) &&
         !g_get_function_body(declaration);
}

static inline astn g_new_varargs_param() {
  astn n = ast_new(ast_declaration);
  astn ctype = ast_new(ast_ctype);
  ctype->ctype.type = TOK_SYM_VARARGS;
  slist_add_head(&n->declaration.type_chain, ctype);
  return n;
}

static inline bool g_is_varargs_param(astn n) {
  assert(n->type == ast_declaration);
  astn ctype = slist_peek_head(&n->declaration.type_chain);
  return ctype->ctype.type == TOK_SYM_VARARGS;
}

static inline bool g_is_function_varargs(astn n) {
  astn params = g_get_function_params(n);
  assert(params);
  assert(params->type == ast_parameters);
  if (g_is_varargs_param(slist_peek_tail(&params->parameters.list))) {
    return true;
  }
  return false;
}

static inline astn g_new_void_param() {
  astn n = ast_new(ast_declaration);
  astn ctype = ast_new(ast_ctype);
  ctype->ctype.type = TOK_KW_VOID;
  slist_add_head(&n->declaration.type_chain, ctype);
  return n;
}

static inline bool g_is_void_param(astn n) {
  assert(n->type == ast_declaration);
  astn ctype = slist_peek_head(&n->declaration.type_chain);
  return ctype->ctype.type == TOK_KW_VOID;
}

static inline bool g_is_function_void_param(astn n) {
  astn params = g_get_function_params(n);
  assert(params);
  assert(params->type == ast_parameters);
  if (g_is_void_param(slist_peek_head(&params->parameters.list))) {
    return true;
  }
  return false;
}

static inline astn g_get_declaration_specifier(astn declaration) {
  assert(declaration->type == ast_declaration);
  return slist_peek_tail(&declaration->declaration.type_chain);
}

static inline astn g_get_declaration_base_type(astn declaration) {
  assert(declaration->type == ast_declaration);
  return slist_peek_head(&declaration->declaration.type_chain);
}
static inline astn g_get_function_return_base_type(astn n) {
  assert(g_get_function_params(n));
  return slist_get(&n->declaration.type_chain, 2)->data;
}
static inline bool g_is_declaration_in_function_scope(astn n) {
  assert(n->type == ast_declaration);
  return n->declaration.scope_ref != NULL;
}
#endif
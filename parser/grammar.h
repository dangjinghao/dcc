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
  return parser_get_typedef(parser, parser->lexer->lex_token._ident) != NULL;
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
 * <struct-or-union-specifier> ::= <struct-or-union>
 * @param parser 
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
  if (ref->type == ast_block) {
    return ref;
  }
  return NULL;
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

static inline bool g_is_varargs(astn n) {
  return n->type == ast_ctype && n->ctype.type == TOK_SYM_VARARGS;
}
#endif
#ifndef GRAMMAR_H
#define GRAMMAR_H
#include "lexer.h"
#include "macro/macro.h"
#include "parser.h"
#include <assert.h>

/**
 * @brief 
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
 * @brief 
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

/**
 * @brief 
 * @grammar
 * <typedef-name> ::= identifier
 * @param parser 
 */
static inline bool g_is_typedef_name_firstset(parser parser) {
  return false;
  BUILDING();
}

/**
 * @brief 
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
 * @brief 
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
 * @brief 
 * @grammar
 * <struct-or-union-specifier> ::= <struct-or-union>
 * @param parser 
 */
static inline bool g_is_struct_or_union_specifier(parser parser) {
  return g_is_struct_or_union_firstset(parser);
}

/**
 * @brief 
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
 * @brief 
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
 * @brief 
 * @grammar
 * <pointer> ::= * {<type-qualifier>}* {<pointer>}?
 * 
 */
static inline bool g_is_pointer_firstset(parser parser) {
  return parser->current_token == '*';
}

/**
 * @brief 
 * @grammar, left-combining and right-recursive
 * <direct-declarator> ::= <identifier>
 *                    | ( <declarator> ) <direct-declarator>
 *                    | [ {<constant-expression>}? ] <direct-declarator>
 *                    |( <parameter-type-list> ) <direct-declarator>
 *                    | ( {<identifier>}* ) <direct-declarator>
 *                    | <epsilon>

 */
static inline bool g_is_direct_declarator_firstset(parser parser) {
  int token = parser->current_token;
  return token == TOK_IDENT || token == '(' || token == '[' || token == TOK_EOF;
}

/**
 * @brief 
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
 * @brief 
 * @grammar 
 * <function-definition> ::= {<declaration-specifier>}+ <declarator> {<declaration>}* <compound-statement>
 * 
 */
static inline bool g_is_function_definition_firstset(parser parser) {
  return g_is_declaration_specifier_firstset(parser);
}

/**
 * @brief 
 * @grammar
 * <declaration> ::=  {<declaration-specifier>}+ {{<init-declarator>} {, <init-declarator>}*}? ;
 */

static inline bool g_is_declaration_firstset(parser parser) {
  return g_is_declaration_specifier_firstset(parser);
}

/**
 * @brief 
 * @grammar
 * <external-declaration> ::= <function-definition>
 *                         | <declaration>
 * @param parser 
 */
static inline bool g_is_external_declaration_firstset(parser parser) {
  return g_is_function_definition_firstset(parser) ||
         g_is_declaration_firstset(parser);
}

/**
 * @brief 
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
 * @brief 
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
 * @brief 
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
  BUILDING();
}

#endif
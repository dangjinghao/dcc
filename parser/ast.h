#ifndef AST_H
#define AST_H

#include "chable/hash_table.h"
#include "lexer.h"
#include "sds/sds.h"
#include <stdlib.h>
enum ast_type {
  ast_expr_unary = 1,
  ast_expr_binop,
  ast_expr_ternary,
  ast_expr_primary,
  ast_trans_unit,
  ast_declaration,
};

enum type_qualifier {
  TYPE_QUAL_NONE = 0,
  TYPE_QUAL_CONST = 1,
  TYPE_QUAL_VOLATILE = 1 << 1,
  TYPE_QUAL_RESTRICT = 1 << 2,
  TYPE_QUAL_INLINE = 1 << 3,
};

typedef struct ctype_node {
  struct ctype_node *next;
  enum type_qualifier qualifier;
  enum tok_type type, signint,
      storage; // trick: fill token_type with 0 or TOK_UNKNOWN
  struct ast_node *user_defined_type; // used for struct, union, enum
} *ctype;

typedef struct ast_node {
  enum ast_type type;
  unsigned int ln, col;
  union {
    sds ident;
    struct unary {
      int op;
      char postfix;
      struct ast_node *expr;
    } unary;
    struct binop {
      int op;
      struct ast_node *lhs, *rhs;
    } binop;
    struct ternary {
      struct ast_node *cond, *_t, *_f;
    } ternary;
    struct primary {
      // tok_lit_*, tok_ident
      enum tok_type type;
      union token v;
    } primary;
    struct trans_unit {
      struct chable symtab;
    } trans_unit;

    struct declaration {
      sds ident;
      // initializer/function body
      struct ast_node *extdata;
      // TODO: type list
    } declaration;
  };
} *astn;

static inline struct ast_node *ast_new(enum ast_type type) {
  struct ast_node *node = calloc(1, sizeof(struct ast_node));
  node->type = type;
  return node;
}

static inline struct ctype_node *ctype_new() {
  return calloc(1, sizeof(struct ctype_node));
}

#endif
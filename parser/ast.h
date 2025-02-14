#ifndef AST_H
#define AST_H

#include "dynarray/dynarray.h"
#include "lexer.h"
#include "sds/sds.h"
#include <stdlib.h>
enum ast_type {
  ast_expr_unary = 1,
  ast_expr_binop,
  ast_expr_ternary,
  ast_expr_primary,
  ast_declaration,
  ast_trans_unit,
  ast_ctype,
  ast_ident,
};

enum type_qualifier {
  TYPE_QUAL_NONE = 0,
  TYPE_QUAL_CONST = 1,
  TYPE_QUAL_VOLATILE = 1 << 1,
  TYPE_QUAL_RESTRICT = 1 << 2,
  TYPE_QUAL_INLINE = 1 << 3,
};

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
      // tok_lit_*
      enum tok_type type;
      union token v;
    } primary;
    struct trans_unit {
      struct dynarray declarations;
    } trans_unit;
    struct ctype {
      enum type_qualifier qualifier;
      // trick: fill token_type with 0 or TOK_UNKNOWN
      enum tok_type type, signint, storage;
      struct ast_node *user_defined_type; // used for struct, union, enum
    } ctype;
    struct declaration {
      sds ident;
      // initializer/function body
      struct ast_node *extdata;
      struct dynarray type_chain;
    } declaration;
  };
} *astn;

static inline struct ast_node *ast_new(enum ast_type type) {
  struct ast_node *node = calloc(1, sizeof(struct ast_node));
  node->type = type;
  return node;
}

#endif
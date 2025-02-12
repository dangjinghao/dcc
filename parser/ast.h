#ifndef AST_H
#define AST_H

#include "lexer.h"
#include "sds/sds.h"
#include <stdlib.h>
enum ast_type {
  ast_expr_unary = 1,
  ast_expr_binop,
  ast_expr_ternary,
  ast_expr_primary,
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
      struct ast_node *cond, *lhs, *rhs;
    } ternary;
    struct {
      // tok_lit_*, tok_ident
      enum tok_type type;
      union token v;
    } value;
  };
} *astn;

static inline struct ast_node *ast_new(enum ast_type type) {
  struct ast_node *node = malloc(sizeof(struct ast_node));
  node->type = type;
  return node;
}

#endif
#ifndef AST_H
#define AST_H

#include "dynarray/dynarray.h"
#include "lexer.h"
#include "sds/sds.h"
enum ast_type {
  ast_expr_unary = 1,
  ast_expr_binop,
  ast_expr_ternary,
  ast_expr_primary,
  ast_declaration,
  ast_block,
  ast_ctype,
  ast_ident,
  ast_labeled_statement,
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
    struct block {
      struct dynarray *stmts;
    } block;
    struct declaration {
      sds ident;
      // initializer/function body
      struct ast_node *extdata;
      struct dynarray *type_chain;
    } declaration;
    struct ctype {
      enum type_qualifier qualifier;
      // trick: fill token_type with 0 or TOK_UNKNOWN
      enum tok_type type, signint, storage;
      // used for struct, union, enum, holds a reference only, do not free it
      struct ast_node *user_defined_type;
    } ctype;
    struct labeled_statement {
      enum tok_type type;
      struct ast_node *label_value;
      struct ast_node *stmt;
    } labeled_statement;
  };
} *astn;

struct ast_node *ast_new(enum ast_type type);
astn ast_new_empty_statement();
void ast_free(astn node);
astn ast_copy(astn n);

#endif
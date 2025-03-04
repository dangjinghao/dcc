#ifndef AST_H
#define AST_H

#include "lexer.h"
#include "sds/sds.h"
#include "slist/slist.h"
#include <llvm-c/Types.h>
enum ast_type {
  ast_expr_unary = 1,
  ast_expr_binop,
  ast_expr_ternary,
  ast_expr_primary,
  ast_expr_typecast,
  ast_declaration,
  ast_ctype,
  ast_ident,
  ast_labeled_statement,
  ast_ref,
  ast_struct_union_declaration,
  ast_parameters,
  ast_arguments,
  ast_block,
  ast_trans_unit,
  ast_initializer,
  ast_initializer_list,
  ast_jump_statement,
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
    struct ast_node *ref;
    sds ident;
    struct unary {
      int op;
      char postfix;
      // used for array subscript(expr type), function call(arguments), get member(by arrow or dot)(ident type)
      struct ast_node *extdata;
      struct ast_node *expr;
    } unary;
    struct typecast {
      struct slist type_chain;
      struct ast_node *expr;
    } typecast;
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
    struct declaration {
      sds ident;
      // initializer/function body/struct declaration bitfield (expr)
      struct ast_node *extdata;
      size_t uid;
      struct ast_node *scope_ref;
      // it contains those node type:
      // ast_expr_unary is used for array declaration [<expr>],
      // ast_parameters is used for function parameters(<parameter-type-list>),
      // ast_ctype
      struct slist type_chain;
      LLVMValueRef V;
    } declaration;
    struct ctype {
      enum type_qualifier qualifier;
      // trick: fill token_type with 0 or TOK_UNKNOWN
      int type;
      enum tok_type signint, storage;
      // used for struct, union, enum, holds a reference only, do not free it
      struct ast_node *user_defined_type;
    } ctype;
    struct labeled_statement {
      enum tok_type type;
      struct ast_node *label_value;
      struct ast_node *stmt;
    } labeled_statement;
    struct struct_union_declaration {
      sds ident;
      size_t uid;
      LLVMTypeRef V;
      // store the struct declaration type in the list
      struct slist member_declarations;
    } struct_union_declaration;

    struct parameters {
      struct slist list;
    } parameters;
    struct block {
      struct slist list;
    } block;
    struct trans_unit {
      struct slist list;
    } trans_unit;
    struct arguments {
      struct slist list;
    } arguments;
    struct initializer_list {
      struct slist list;
    } initializer_list;
    struct initializer {
      // expr or initializer_list
      struct ast_node *init;
    } initializer;
    struct jump_statement {
      enum tok_type type;
      struct ast_node *target_block;
      struct ast_node *expr;
    } jump_statement;
  };
} *astn;

struct ast_node *ast_new(enum ast_type type);
void ast_free(astn node);
astn ast_copy(astn n);

#endif
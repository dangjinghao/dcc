#include "ast.h"
#include "builder.h"
#include "grammar.h"
#include "lexer.h"
#include "macro/macro.h"
#include "slist/slist.h"
#include "typed_value/typed_value.h"
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>

/**
 * @brief the sub-branch of build_expression which handles the '+' operator with type cast 
 * 
 * @param b 
 * @param binop 
 * @return llvm_typed_value 
 */
llvm_typed_value build_expr_binop_plus(builder b, astn binop) {
  llvm_typed_value lhs = build_expression(b, binop->binop.lhs);
  llvm_typed_value rhs = build_expression(b, binop->binop.rhs);
  llvm_typed_value *exprs =
      build_2_values_type_upper_cast(b, (llvm_typed_value[]){lhs, rhs});
  astn base_type = slist_peek_head(&exprs[0]->type_chain);
  if (g_is_int_family_tok(base_type->ctype.type)) {
    return llvm_typed_value_new(
        LLVMBuildAdd(b->builder, exprs[0]->v, exprs[1]->v, "iadd"),
        &exprs[0]->type_chain);
  } else if (base_type->ctype.type == TOK_KW_FLOAT ||
             base_type->ctype.type == TOK_KW_DOUBLE) {
    return llvm_typed_value_new(
        LLVMBuildFAdd(b->builder, exprs[0]->v, exprs[1]->v, "fadd"),
        &exprs[0]->type_chain);
  }
  BUILDING();
}

/**
 * @brief the sub-branch of build_expression
 * 
 * @param b 
 * @param n 
 * @return llvm_typed_value 
 */
llvm_typed_value build_expr_binop(builder b, astn n) {
  switch (n->binop.op) {
  case ',': {
    build_expression(b, n->binop.lhs);
    return build_expression(b, n->binop.rhs);
  }
  case '+': {
    return build_expr_binop_plus(b, n);
  }
}
  BUILDING();
}

llvm_typed_value build_expr_unary(builder b, astn n) {
  if (n->unary.postfix) {
    switch (n->unary.op) {
    case '+':
    case TOK_SYM_SELF_INC:
    case TOK_SYM_SELF_DEC:
    case TOK_KW_SIZEOF:
    case '-':
    case '!':
    case '~':
    case '*':
    case '&':
      break;
    }
  } else {
    switch (n->unary.op) {
    case TOK_SYM_SELF_INC:
    case TOK_SYM_SELF_DEC:
    case TOK_SYM_ARROW:
    case '[':
    case '(':
    case '.':
      break;
    }
  }
  BUILDING();
}

llvm_typed_value build_expr_primary(builder b, astn n) {
  switch (n->primary.type) {
  case TOK_LIT_INT: {
    return llvm_typed_value_new(LLVMConstInt(LLVMInt32TypeInContext(b->context),
                                             n->primary.v._int, true),
                                build_type_chain_expr_primary(n));
  }
  case TOK_LIT_UINT: {
    return llvm_typed_value_new(LLVMConstInt(LLVMInt32TypeInContext(b->context),
                                             n->primary.v._uint, false),
                                build_type_chain_expr_primary(n));
  }
  case TOK_LIT_LONG: {
    return llvm_typed_value_new(LLVMConstInt(LLVMInt64TypeInContext(b->context),
                                             n->primary.v._int, true),
                                build_type_chain_expr_primary(n));
  }
  case TOK_LIT_ULONG: {
    return llvm_typed_value_new(LLVMConstInt(LLVMInt64TypeInContext(b->context),
                                             n->primary.v._int, false),
                                build_type_chain_expr_primary(n));
  }
  case TOK_LIT_FLOAT:
    return llvm_typed_value_new(
        LLVMConstReal(LLVMFloatTypeInContext(b->context), n->primary.v._float),
        build_type_chain_expr_primary(n));
  case TOK_LIT_DOUBLE:
    return llvm_typed_value_new(
        LLVMConstReal(LLVMDoubleTypeInContext(b->context),
                      n->primary.v._double),
        build_type_chain_expr_primary(n));
  case TOK_LIT_CHAR:
    return llvm_typed_value_new(LLVMConstInt(LLVMInt8TypeInContext(b->context),
                                             n->primary.v._char, true),
                                build_type_chain_expr_primary(n));
  case TOK_LIT_STRING:
  default:
    BUILDING();
  }
}

llvm_typed_value build_expression(builder b, astn n) {
  switch (n->type) {
  case ast_expr_binop: {
    return build_expr_binop(b, n);
  }
  case ast_expr_unary: {
    return build_expr_unary(b, n);
  }
  case ast_expr_primary: {
    return build_expr_primary(b, n);
  }
  default:
  }
  BUILDING();
}

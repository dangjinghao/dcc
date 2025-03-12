#include "ast.h"
#include "builder.h"
#include "grammar.h"
#include "lexer.h"
#include "log/log.h"
#include "macro/macro.h"
#include "slist/slist.h"
#include "typed_value/typed_value.h"
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>

typedef LLVMValueRef (*llvm_func_t)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef,
                                    const char *);
/**
 * @brief 
 * 
 * @param b 
 * @param binop 
 * @param llvm_build_f [0]: int function, [1]: fp function
 * @param f_names 
 * @return llvm_typed_value 
 */
llvm_typed_value build_expr_binop_template(builder b, astn binop,
                                           llvm_func_t llvm_build_f[2],
                                           char *f_names[2]) {
  llvm_typed_value lhs = build_expression(b, binop->binop.lhs);
  llvm_typed_value rhs = build_expression(b, binop->binop.rhs);
  llvm_typed_value *exprs =
      build_2_values_type_upper_cast(b, (llvm_typed_value[]){lhs, rhs});
  astn base_type = slist_peek_head(&exprs[0]->type_chain);
  if (g_is_int_family_tok(base_type->ctype.type)) {
    return llvm_typed_value_new(
        llvm_build_f[0](b->builder, exprs[0]->v, exprs[1]->v, f_names[0]),
        &exprs[0]->type_chain);
  } else if (g_is_fp_family_tok(base_type->ctype.type)) {
    return llvm_typed_value_new(
        llvm_build_f[1](b->builder, exprs[0]->v, exprs[1]->v, f_names[1]),
        &exprs[0]->type_chain);
  }
  BUILDING();
}

llvm_typed_value build_expr_binop_plus(builder b, astn binop) {
  llvm_func_t llvm_build_f[2] = {LLVMBuildAdd, LLVMBuildFAdd};
  char *f_names[2] = {"iadd", "fadd"};
  return build_expr_binop_template(b, binop, llvm_build_f, f_names);
}

llvm_typed_value build_expr_binop_sub(builder b, astn binop) {
  llvm_func_t llvm_build_f[2] = {LLVMBuildSub, LLVMBuildFSub};
  char *f_names[2] = {"isub", "fsub"};
  return build_expr_binop_template(b, binop, llvm_build_f, f_names);
}

llvm_typed_value build_expr_binop_mul(builder b, astn binop) {
  llvm_func_t llvm_build_f[2] = {LLVMBuildMul, LLVMBuildFMul};
  char *f_names[2] = {"imul", "fmul"};
  return build_expr_binop_template(b, binop, llvm_build_f, f_names);
}

llvm_typed_value build_expr_binop_div(builder b, astn binop) {
  llvm_typed_value lhs = build_expression(b, binop->binop.lhs);
  llvm_typed_value rhs = build_expression(b, binop->binop.rhs);
  llvm_typed_value *exprs =
      build_2_values_type_upper_cast(b, (llvm_typed_value[]){lhs, rhs});
  astn base_type = slist_peek_head(&exprs[0]->type_chain);
  if (g_is_int_family_tok(base_type->ctype.type) &&
      base_type->ctype.signint == TOK_KW_SIGNED) {
    return llvm_typed_value_new(
        LLVMBuildSDiv(b->builder, exprs[0]->v, exprs[1]->v, "sdiv"),
        &exprs[0]->type_chain);
  } else if (g_is_int_family_tok(base_type->ctype.type) &&
             base_type->ctype.signint == TOK_KW_UNSIGNED) {
    return llvm_typed_value_new(
        LLVMBuildUDiv(b->builder, exprs[0]->v, exprs[1]->v, "udiv"),
        &exprs[0]->type_chain);
  } else if (g_is_fp_family_tok(base_type->ctype.type)) {
    return llvm_typed_value_new(
        LLVMBuildFDiv(b->builder, exprs[0]->v, exprs[1]->v, "fdiv"),
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
  case '-': {
    return build_expr_binop_sub(b, n);
  }
  case '*': {
    return build_expr_binop_mul(b, n);
  }
  case '/': {
    return build_expr_binop_div(b, n);
  }
  }

  BUILDING();
}

llvm_typed_value build_expr_unary_pos(builder b, astn n) {
  llvm_typed_value expr = build_expression(b, n);
  // tiny int -> int
  astn expr_base_type = slist_peek_head(&expr->type_chain);
  // create a temporary int type and its corresponsed type chain
  slist int_type_chain = build_base_type_chain_by_lit(TOK_LIT_INT);

  int cmp = build_type_compare_promote_level(expr_base_type,
                                             slist_peek_head(int_type_chain));
  if (cmp == -1) {
    log_trace("+ unary operator type promotion: tiny int -> int");
    llvm_typed_value v = build_convert_type_to(b, expr, int_type_chain);
    return v;
  }
  return expr;
}

llvm_typed_value build_expr_unary_not(builder b, astn n) {
  // neq 0 then ext to i8
  llvm_typed_value expr = build_expression(b, n);
  astn base_type = slist_peek_head(&expr->type_chain);
  LLVMValueRef eq0;
  if (g_is_int_family_tok(base_type->ctype.type)) {
    eq0 = LLVMBuildICmp(
        b->builder, LLVMIntEQ, expr->v,
        LLVMConstInt(build_convert_base_type(b, base_type), 0, false), "ieq0");

  } else if (g_is_fp_family_tok(base_type->ctype.type)) {
    eq0 = LLVMBuildFCmp(b->builder, LLVMRealUEQ, expr->v,
                        LLVMConstReal(build_convert_base_type(b, base_type), 0),
                        "feq0");
  } else {
    BUILDING();
  }
  auto zext = LLVMBuildZExt(b->builder, eq0, LLVMInt8TypeInContext(b->context),
                            "zexteq0");
  return llvm_typed_value_new(zext, build_base_type_chain_by_lit(TOK_LIT_CHAR));
}

llvm_typed_value build_expr_unary_neg(builder b, astn n) {
  llvm_typed_value expr = build_expression(b, n);
  // negation
  astn base_type = slist_peek_head(&expr->type_chain);
  if (g_is_int_family_tok(base_type->ctype.type)) {
    return llvm_typed_value_new(LLVMBuildNeg(b->builder, expr->v, "neg"),
                                &expr->type_chain);
  } else if (g_is_fp_family_tok(base_type->ctype.type)) {
    return llvm_typed_value_new(LLVMBuildFNeg(b->builder, expr->v, "fneg"),
                                &expr->type_chain);
  }
  BUILDING();
}

llvm_typed_value build_expr_unary(builder b, astn n) {
  if (!n->unary.postfix) {
    // suffix
    switch (n->unary.op) {
    case '+': {
      return build_expr_unary_pos(b, n->unary.expr);
    }
    case '-': {
      return build_expr_unary_neg(b, n->unary.expr);
    }
    case '!': {
      return build_expr_unary_not(b, n->unary.expr);
    }
    case TOK_SYM_SELF_INC:
    case TOK_SYM_SELF_DEC:
    case TOK_KW_SIZEOF:
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

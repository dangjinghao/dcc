#include "ast.h"
#include "builder.h"
#include "convert/convert.h"
#include "dynarray/dynarray.h"
#include "grammar.h"
#include "lexer.h"
#include "log/log.h"
#include "macro/macro.h"
#include "parser.h"
#include "slist/slist.h"
#include "token.h"
#include "typed_value/typed_value.h"
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>

bool build_expr_is_binop_with_ptr(typed_value lhs, typed_value rhs) {
  astn lhs_base_type = slist_peek_head(&lhs->type_chain);
  astn rhs_base_type = slist_peek_head(&rhs->type_chain);
  if (lhs_base_type->ctype.type == '*' || rhs_base_type->ctype.type == '*') {
    return true;
  }
  return false;
}

/**
 * @brief Only support: ptr +/- int, ptr - ptr, int + ptr,
 * 
 * @param b 
 * @param binop 
 * @return typed_value 
 */
typed_value build_expr_binop_ptr(builder b, typed_value lhs, int op,
                                 typed_value rhs) {
  if (op != '+' && op != '-') {
    goto FAIL;
  }
  astn lhs_base_type = slist_peek_head(&lhs->type_chain);
  astn rhs_base_type = slist_peek_head(&rhs->type_chain);
  if (lhs_base_type->ctype.type == '*' && rhs_base_type->ctype.type == '*' &&
      op == '-') {
    // ptr - ptr
    // convert to int
    LLVMValueRef lhs_int = LLVMBuildPtrToInt(
        b->builder, lhs->v, LLVMInt64TypeInContext(b->context), "lhs_ptrtoint");
    LLVMValueRef rhs_int = LLVMBuildPtrToInt(
        b->builder, rhs->v, LLVMInt64TypeInContext(b->context), "rhs_ptrtoint");
    // sub
    LLVMValueRef sub = LLVMBuildSub(b->builder, lhs_int, rhs_int, "ptrsub");
    // sdiv
    slist item_type_chain =
        build_type_get_points_to_type_chian(b, &lhs->type_chain);
    astn item_base_type = slist_peek_head(item_type_chain);
    LLVMValueRef size =
        LLVMConstInt(LLVMInt64TypeInContext(b->context),
                     lexer_token_get_sizeof(item_base_type->ctype.type), false);
    LLVMValueRef result =
        LLVMBuildSDiv(b->builder, sub, size, "ptr_item_size_sdiv");
    return typed_value_new(result, item_type_chain);
  } else if ((g_is_int_family_tok(lhs_base_type->ctype.type) ||
              g_is_int_family_tok(rhs_base_type->ctype.type)) &&
             op == '+') {
    // int + ptr or ptr + int
    typed_value ptr;
    typed_value index;
    if (g_is_int_family_tok(lhs_base_type->ctype.type)) {
      // int + ptr
      ptr = rhs;
      index = lhs;
    } else {
      // ptr + int
      ptr = lhs;
      index = rhs;
    }

    slist item_type_chain =
        build_type_get_points_to_type_chian(b, &ptr->type_chain);
    LLVMTypeRef item_type =
        build_type_ctype_convert_to_llvm(b, slist_peek_head(item_type_chain));
    // cast index to i64 if it is not
    slist tmp_long_type_chain = build_type_chain_by_lit(TOK_LIT_LONG);
    index = build_type_convert_by_type_chain(b, index, tmp_long_type_chain);

    LLVMValueRef result = LLVMBuildGEP2(b->builder, item_type, ptr->v,
                                        &index->v, 1, "ptr_plus_int");
    return typed_value_new(result, &ptr->type_chain);
  } else if (lhs_base_type->ctype.type == '*' &&
             g_is_int_family_tok(rhs_base_type->ctype.type) && op == '-') {
    // ptr - int
    // use getelementptr
    slist item_type_chain =
        build_type_get_points_to_type_chian(b, &lhs->type_chain);
    LLVMTypeRef item_type =
        build_type_ctype_convert_to_llvm(b, slist_peek_head(item_type_chain));
    typed_value idx = rhs;
    slist tmp_long_type_chain = build_type_chain_by_lit(TOK_LIT_LONG);
    idx = build_type_convert_by_type_chain(b, idx, tmp_long_type_chain);
    idx->v = LLVMBuildNeg(b->builder, rhs->v, "neg");
    LLVMValueRef result = LLVMBuildGEP2(b->builder, item_type, lhs->v, &idx->v,
                                        1, "ptr_minus_int");
    return typed_value_new(result, &lhs->type_chain);
  }
  // unexpected expression format
FAIL:
  log_panic("Only support expression operation like: <ptr-type> +/- <int>, "
            "<ptr-type> - <ptr-type> and "
            "<int> + <ptr-type>");
}

/**
 * @brief 
 * 
 * @param b 
 * @param binop 
 * @param llvm_build_f [0]: int  [1]: fp
 * @param f_names 
 * @return typed_value 
 */
typed_value build_expr_binop_template(builder b, astn binop,
                                      llvm_func_t llvm_build_f[2],
                                      char *f_names[2]) {
  typed_value lhs = build_expression(b, binop->binop.lhs);
  typed_value rhs = build_expression(b, binop->binop.rhs);
  if (build_expr_is_binop_with_ptr(lhs, rhs)) {
    log_trace("ptr operation detected in binop expression");
    return build_expr_binop_ptr(b, lhs, binop->binop.op, rhs);
  }
  return build_value_expr_binop_template(b, lhs, rhs, llvm_build_f, f_names);
}

typed_value build_expr_binop_div(builder b, astn binop) {
  typed_value lhs = build_expression(b, binop->binop.lhs);
  typed_value rhs = build_expression(b, binop->binop.rhs);
  return build_value_expr_binop_div(b, lhs, rhs);
}

/**
 * @brief Template for binary operations with different handling for signed/unsigned integers
 * 
 * @param b Builder context
 * @param binop Binary operation AST node
 * @param llvm_build_f [0]: signed int function, [1]: unsigned int function
 * @param f_names Names for the operations
 * @return typed_value Result of the operation
 */
typed_value build_expr_binop_su_template(builder b, astn binop,
                                         llvm_func_t llvm_build_f[2],
                                         char *f_names[2]) {
  typed_value lhs = build_expression(b, binop->binop.lhs);
  typed_value rhs = build_expression(b, binop->binop.rhs);
  return build_value_expr_binop_su_template(b, lhs, rhs, llvm_build_f, f_names);
}

typed_value build_expr_binop_bit_template(builder b, astn binop,
                                          llvm_func_t llvm_build_f,
                                          char *f_name) {
  typed_value lhs = build_expression(b, binop->binop.lhs);
  typed_value rhs = build_expression(b, binop->binop.rhs);
  return build_value_expr_binop_bit_template(b, lhs, rhs, llvm_build_f, f_name);
}

/**
 * @brief 
 * 
 * @param b 
 * @param binop 
 * @param int_preds [0]: int predicate, [1]: unsigned int predicate
 * @param fp_pred 
 * @param pred_names [0]: int predicate name, [1]: unsigned int predicate name, [2]: fp predicate name
 * @return typed_value 
 */
typed_value build_expr_binop_logic_cmp(builder b, astn binop, int preds[3],
                                       char *pred_names[3]) {
  typed_value lhs = build_expression(b, binop->binop.lhs);
  typed_value rhs = build_expression(b, binop->binop.rhs);
  astn lhs_base_type = slist_peek_head(&lhs->type_chain);
  astn rhs_base_type = slist_peek_head(&rhs->type_chain);
  // convert ptr to i64 if needed
  if (lhs_base_type->ctype.type == '*') {
    lhs = build_type_convert_by_type_chain(
        b, lhs, build_type_chain_by_lit(TOK_LIT_ULONG));
    lhs_base_type = slist_peek_head(&lhs->type_chain);
  }
  if (rhs_base_type->ctype.type == '*') {
    rhs = build_type_convert_by_type_chain(
        b, rhs, build_type_chain_by_lit(TOK_LIT_ULONG));
    rhs_base_type = slist_peek_head(&rhs->type_chain);
  }

  typed_value *exprs =
      build_type_2_values_type_upper_cast(b, (typed_value[]){lhs, rhs});
  lhs_base_type = slist_peek_head(&exprs[0]->type_chain);
  LLVMValueRef result;
  if (g_is_int_family_tok(lhs_base_type->ctype.type)) {
    if (lhs_base_type->ctype.signint == TOK_KW_SIGNED) {
      result = LLVMBuildICmp(b->builder, preds[0], exprs[0]->v, exprs[1]->v,
                             pred_names[0]);
    } else {
      result = LLVMBuildICmp(b->builder, preds[1], exprs[0]->v, exprs[1]->v,
                             pred_names[1]);
    }
  } else if (g_is_fp_family_tok(lhs_base_type->ctype.type)) {
    result = LLVMBuildFCmp(b->builder, preds[2], exprs[0]->v, exprs[1]->v,
                           pred_names[2]);
  } else {
    log_panic("Unsupported type in logic compare operation:%s",
              convert_repr_ast_type(lhs_base_type->ctype.type));
  }
  // convert i1 to i8
  return typed_value_new(LLVMBuildZExt(b->builder, result,
                                       LLVMInt8TypeInContext(b->context),
                                       "zext_logic_cmp"),
                         build_type_chain_by_lit(TOK_LIT_CHAR));
}

typed_value build_expr_binop_assign(builder b, astn binop) {
  typed_value lhs = build_lvalue_expression(b, binop->binop.lhs);
  typed_value rhs = build_expression(b, binop->binop.rhs);
  astn rhs_base_type = slist_peek_head(&rhs->type_chain);
  if (rhs_base_type->type == ast_ctype &&
      g_is_struct_or_union_token(rhs_base_type->ctype.type) &&
      binop->binop.op != '=') {
    log_panic("Unsupported self assign operation on struct or union type");
  }
  switch (binop->binop.op) {
  case TOK_SYM_SELF_ADD: {
    typed_value lhs_load = build_value_load(b, lhs);
    astn load_base_type = slist_peek_head(&lhs_load->type_chain);
    if (load_base_type->ctype.type == '*') {
      log_trace("ptr operation detected in += expression");
      rhs = build_expr_binop_ptr(b, lhs_load, '+', rhs);
    } else {
      rhs = build_value_expr_binop_template(
          b, lhs_load, rhs, (llvm_func_t[]){LLVMBuildAdd, LLVMBuildFAdd},
          (char *[]){"selfadd", "selffadd"});
    }
    break;
  }
  case TOK_SYM_SELF_SUB: {
    typed_value lhs_load = build_value_load(b, lhs);
    astn load_base_type = slist_peek_head(&lhs_load->type_chain);
    if (load_base_type->ctype.type == '*') {
      log_trace("ptr operation detected in -=  expression");
      rhs = build_expr_binop_ptr(b, lhs_load, '-', rhs);
    } else {
      rhs = build_value_expr_binop_template(
          b, lhs_load, rhs, (llvm_func_t[]){LLVMBuildSub, LLVMBuildFSub},
          (char *[]){"selfsub", "selffsub"});
    }
    break;
  }
  case TOK_SYM_SELF_MUL: {
    rhs = build_value_expr_binop_template(
        b, build_value_load(b, lhs), rhs,
        (llvm_func_t[]){LLVMBuildMul, LLVMBuildFMul},
        (char *[]){"selfmul", "selffmul"});
    break;
  }
  case TOK_SYM_SELF_DIV: {
    rhs = build_value_expr_binop_div(b, build_value_load(b, lhs), rhs);
    break;
  }
  case TOK_SYM_SELF_MOD: {
    rhs = build_value_expr_binop_su_template(
        b, build_value_load(b, lhs), rhs,
        (llvm_func_t[]){LLVMBuildSRem, LLVMBuildURem},
        (char *[]){"selfsrem", "selfurem"});
    break;
  }
  case TOK_SYM_SELF_LSHIFT: {
    rhs = build_value_expr_binop_bit_template(b, build_value_load(b, lhs), rhs,
                                              LLVMBuildShl, "selfshl");
    break;
  }
  case TOK_SYM_SELF_RSHIFT: {
    rhs = build_value_expr_binop_su_template(
        b, build_value_load(b, lhs), rhs,
        (llvm_func_t[]){LLVMBuildShl, LLVMBuildLShr},
        (char *[]){"selfshl", "selflshr"});
    break;
  }
  case TOK_SYM_SELF_BIT_AND: {
    rhs = build_value_expr_binop_bit_template(b, build_value_load(b, lhs), rhs,
                                              LLVMBuildAnd, "selfbitand");
    break;
  }
  case TOK_SYM_SELF_BIT_OR: {
    rhs = build_value_expr_binop_bit_template(b, build_value_load(b, lhs), rhs,
                                              LLVMBuildOr, "selfbitor");
    break;
  }
  case TOK_SYM_SELF_BIT_XOR: {
    rhs = build_value_expr_binop_bit_template(b, build_value_load(b, lhs), rhs,
                                              LLVMBuildXor, "selfbitxor");
    break;
  }
  }
  build_value_store(b, rhs, lhs);
  // loadlhs again for return
  return build_value_load(b, lhs);
}

typed_value build_expr_ternary(builder b, astn ternary) {
  assert(ternary->type == ast_expr_ternary);
  typed_value cond = build_expression(b, ternary->ternary.cond);
  auto test = build_value_ne0(b, cond);
  LLVMBasicBlockRef true_block =
      LLVMAppendBasicBlockInContext(b->context, b->fn, "ternary_true");
  LLVMBasicBlockRef false_block =
      LLVMAppendBasicBlockInContext(b->context, b->fn, "ternary_false");
  LLVMBasicBlockRef merge_block =
      LLVMAppendBasicBlockInContext(b->context, b->fn, "ternary_merge");
  LLVMBuildCondBr(b->builder, test, true_block, false_block);
  // true block
  LLVMPositionBuilderAtEnd(b->builder, true_block);
  typed_value true_expr = build_expression(b, ternary->ternary._t);
  // update true block which maybe updated by sub-expression
  true_block = LLVMGetInsertBlock(b->builder);
  // false block
  LLVMPositionBuilderAtEnd(b->builder, false_block);
  typed_value false_expr = build_expression(b, ternary->ternary._f);
  // update false block which maybe updated by sub-expression
  false_block = LLVMGetInsertBlock(b->builder);

  // type cast
  slist true_type_chain = &true_expr->type_chain;
  slist false_type_chain = &false_expr->type_chain;
  astn true_ty = slist_peek_head(true_type_chain);
  astn false_ty = slist_peek_head(false_type_chain);

  int promt_cmp = build_type_compare_promote_level(true_ty, false_ty);
  if (promt_cmp == 0) {
    log_trace("no need to cast in ternary special case");
  } else if (promt_cmp < 0) {
    log_trace("casting true expr in ternary in ternary special case");
    LLVMPositionBuilderAtEnd(b->builder, true_block);
    true_expr =
        build_type_convert_by_type_chain(b, true_expr, false_type_chain);
  } else {
    log_trace("casting false expr in ternary in ternary special case");
    LLVMPositionBuilderAtEnd(b->builder, false_block);
    false_expr =
        build_type_convert_by_type_chain(b, false_expr, true_type_chain);
  }
  // add br to all branchs
  LLVMPositionBuilderAtEnd(b->builder, true_block);
  LLVMBuildBr(b->builder, merge_block);
  LLVMPositionBuilderAtEnd(b->builder, false_block);
  LLVMBuildBr(b->builder, merge_block);

  // merge block, phi
  LLVMPositionBuilderAtEnd(b->builder, merge_block);
  LLVMValueRef phi =
      LLVMBuildPhi(b->builder,
                   build_type_ctype_convert_to_llvm(
                       b, slist_peek_head(&true_expr->type_chain)),
                   "ternary_phi");
  LLVMAddIncoming(phi, (LLVMValueRef[]){true_expr->v, false_expr->v},
                  (LLVMBasicBlockRef[]){true_block, false_block}, 2);
  return typed_value_new(phi, &true_expr->type_chain);
}

typed_value build_expr_binop_logic_short_circuit(builder b, astn binop,
                                                 bool is_and) {
  typed_value lhs = build_expression(b, binop->binop.lhs);
  LLVMValueRef lhs_check = build_value_ne0(b, lhs);
  LLVMBasicBlockRef start_block = LLVMGetInsertBlock(b->builder);
  LLVMBasicBlockRef next_block = LLVMAppendBasicBlockInContext(
      b->context, b->fn, is_and ? "and_next" : "or_next");
  LLVMBasicBlockRef merge_block = LLVMAppendBasicBlockInContext(
      b->context, b->fn, is_and ? "and_merge" : "or_merge");

  if (is_and) {
    LLVMBuildCondBr(b->builder, lhs_check, next_block, merge_block);
  } else {
    LLVMBuildCondBr(b->builder, lhs_check, merge_block, next_block);
  }

  LLVMPositionBuilderAtEnd(b->builder, next_block);
  typed_value rhs = build_expression(b, binop->binop.rhs);
  LLVMValueRef rhs_check = build_value_ne0(b, rhs);
  next_block = LLVMGetInsertBlock(b->builder);
  LLVMBuildBr(b->builder, merge_block);

  LLVMPositionBuilderAtEnd(b->builder, merge_block);
  LLVMValueRef phi = LLVMBuildPhi(b->builder, LLVMInt1TypeInContext(b->context),
                                  is_and ? "logic_and_phi" : "logic_or_phi");
  LLVMAddIncoming(phi, (LLVMValueRef[]){lhs_check, rhs_check},
                  (LLVMBasicBlockRef[]){start_block, next_block}, 2);

  LLVMValueRef ext =
      LLVMBuildZExt(b->builder, phi, LLVMInt8TypeInContext(b->context),
                    is_and ? "zext_and" : "zext_or");
  return typed_value_new(ext, build_type_chain_by_lit(TOK_LIT_CHAR));
}

/**
 * @brief the sub-branch of build_expression
 * 
 * @param b 
 * @param n 
 * @return typed_value 
 */
typed_value build_expr_binop(builder b, astn n) {
  switch (n->binop.op) {
  case ',': {
    build_expression(b, n->binop.lhs);
    return build_expression(b, n->binop.rhs);
  }
  case '+': {
    return build_expr_binop_template(
        b, n, (llvm_func_t[]){LLVMBuildAdd, LLVMBuildFAdd},
        (char *[]){"add", "fadd"});
  }
  case '-': {
    return build_expr_binop_template(
        b, n, (llvm_func_t[]){LLVMBuildSub, LLVMBuildFSub},
        (char *[]){"sub", "fsub"});
  }
  case '*': {
    return build_expr_binop_template(
        b, n, (llvm_func_t[]){LLVMBuildMul, LLVMBuildFMul},
        (char *[]){"mul", "fmul"});
  }
  case '/': {
    return build_expr_binop_div(b, n);
  }
  case '<': {
    return build_expr_binop_logic_cmp(
        b, n, (int[]){LLVMIntSLT, LLVMIntULT, LLVMRealOLT},
        (char *[]){"lt", "ult", "olt"});
  }
  case '>': {
    return build_expr_binop_logic_cmp(
        b, n, (int[]){LLVMIntSGT, LLVMIntUGT, LLVMRealOGT},
        (char *[]){"gt", "ugt", "ogt"});
  }
  case TOK_SYM_LEQ: {
    return build_expr_binop_logic_cmp(
        b, n, (int[]){LLVMIntSLE, LLVMIntULE, LLVMRealOLE},
        (char *[]){"le", "ule", "ole"});
  }
  case TOK_SYM_GEQ: {
    return build_expr_binop_logic_cmp(
        b, n, (int[]){LLVMIntSGE, LLVMIntUGE, LLVMRealOGE},
        (char *[]){"ge", "uge", "oge"});
  }
  case TOK_SYM_NEQ: {
    return build_expr_binop_logic_cmp(
        b, n, (int[]){LLVMIntNE, LLVMIntNE, LLVMRealONE},
        (char *[]){"ne", "ne", "one"});
  }
  case TOK_SYM_EQ: {
    return build_expr_binop_logic_cmp(
        b, n, (int[]){LLVMIntEQ, LLVMIntEQ, LLVMRealOEQ},
        (char *[]){"eq", "eq", "oeq"});
  }
  case '&': {
    return build_expr_binop_bit_template(b, n, LLVMBuildAnd, "bitand");
  }
  case '|': {
    return build_expr_binop_bit_template(b, n, LLVMBuildOr, "bitor");
  }
  case '^': {
    return build_expr_binop_bit_template(b, n, LLVMBuildXor, "bitxor");
  }
  case TOK_SYM_LSHIFT: {
    return build_expr_binop_bit_template(b, n, LLVMBuildShl, "shl");
  }
  case '%': {
    return build_expr_binop_su_template(
        b, n, (llvm_func_t[]){LLVMBuildSRem, LLVMBuildURem},
        (char *[]){"srem", "urem"});
  }
  case TOK_SYM_RSHIFT: {
    return build_expr_binop_su_template(
        b, n, (llvm_func_t[]){LLVMBuildAShr, LLVMBuildLShr},
        (char *[]){"ashr", "lshr"});
  }
  case TOK_SYM_SELF_BIT_AND:
  case TOK_SYM_SELF_BIT_OR:
  case TOK_SYM_SELF_BIT_XOR:
  case TOK_SYM_SELF_DIV:
  case TOK_SYM_SELF_LSHIFT:
  case TOK_SYM_SELF_MOD:
  case TOK_SYM_SELF_MUL:
  case TOK_SYM_SELF_RSHIFT:
  case TOK_SYM_SELF_ADD:
  case TOK_SYM_SELF_SUB:
  case '=': {
    return build_expr_binop_assign(b, n);
  }
  case TOK_SYM_LOGIC_AND: {
    return build_expr_binop_logic_short_circuit(b, n, true);
  }
  case TOK_SYM_LOGIC_OR: {
    return build_expr_binop_logic_short_circuit(b, n, false);
  }
  }
  log_panic("Unsupported binary operation:%c", convert_repr_token(n->binop.op));
}

typed_value build_expr_unary_pos(builder b, astn n) {
  typed_value expr = build_expression(b, n);
  // panic if the type is not numeric
  astn base_type = slist_peek_head(&expr->type_chain);
  if (!g_is_numeric_tok(base_type->ctype.type)) {
    log_panic("Unary positive operation is only allowed on numeric types");
  }
  // tiny int -> int
  astn expr_base_type = slist_peek_head(&expr->type_chain);
  // create a temporary int type and its corresponsed type chain
  slist tmp_type_chain = build_type_chain_by_lit(TOK_LIT_INT);

  int cmp = build_type_compare_promote_level(expr_base_type,
                                             slist_peek_head(tmp_type_chain));
  if (cmp == -1) {
    log_trace("+ unary operator type promotion: tiny int -> int");
    typed_value v = build_type_convert_by_type_chain(b, expr, tmp_type_chain);
    return v;
  }
  return expr;
}

typed_value build_expr_unary_not(builder b, astn n) {

  auto eq0 = build_value_eq0(b, build_expression(b, n));
  auto zext = LLVMBuildZExt(b->builder, eq0, LLVMInt8TypeInContext(b->context),
                            "zext_unary_not");
  return typed_value_new(zext, build_type_chain_by_lit(TOK_LIT_CHAR));
}

typed_value build_expr_unary_neg(builder b, astn n) {
  typed_value expr = build_expression(b, n);
  astn base_type = slist_peek_head(&expr->type_chain);

  if (g_is_int_family_tok(base_type->ctype.type)) {
    return typed_value_new(LLVMBuildNeg(b->builder, expr->v, "neg"),
                           &expr->type_chain);
  } else if (g_is_fp_family_tok(base_type->ctype.type)) {
    return typed_value_new(LLVMBuildFNeg(b->builder, expr->v, "fneg"),
                           &expr->type_chain);
  }
  log_panic("Unary negative operation is only allowed on numeric types");
}

typed_value build_expr_unary_bit_not(builder b, astn n) {
  typed_value expr = build_expression(b, n);
  // bit not
  astn base_type = slist_peek_head(&expr->type_chain);
  if (!g_is_int_family_tok(base_type->ctype.type)) {
    log_error("unary bit not operation is only allowed on int type");
  }
  return typed_value_new(LLVMBuildNot(b->builder, expr->v, "bitnot"),
                         &expr->type_chain);
}

typed_value build_expr_unary_deref(builder b, astn n) {
  // deref
  typed_value expr = build_expression(b, n);
  // check if the type is a pointer
  astn base_type = slist_peek_head(&expr->type_chain);
  if (base_type->ctype.type != '*') {
    log_panic("Unary dereference operation is only allowed on pointer types");
  }
  return build_value_load(b, expr);
}

/**
 * @brief Self increment/decrement operation which supports both prefix and postfix
 * 
 * @param b 
 * @param n 
 * @param t 
 * @param postfix 
 * @return typed_value 
 */
typed_value build_expr_unary_self_inc(builder b, astn n, enum tok_type t,
                                      bool postfix) {
  typed_value expr = build_lvalue_expression(b, n);
  // Get the value pointed to
  slist points_to_type_chain =
      build_type_get_points_to_type_chian(b, &expr->type_chain);
  LLVMTypeRef points_to_type = build_type_ctype_convert_to_llvm(
      b, slist_peek_head(points_to_type_chain));

  // Load current value
  LLVMValueRef old = build_value_load(b, expr)->v;
  // Create the constant for incrementing (1)
  LLVMValueRef one;
  astn base_type = slist_peek_head(points_to_type_chain);
  assert(base_type->type == ast_ctype);
  if (g_is_int_family_tok(base_type->ctype.type)) {
    one = LLVMConstInt(points_to_type, 1, false);
  } else if (base_type->ctype.type == '*') {
    one = LLVMConstInt(LLVMInt64TypeInContext(b->context), 1, false);
  } else if (g_is_fp_family_tok(base_type->ctype.type)) {
    one = LLVMConstReal(points_to_type, 1.0);
  } else {
    log_panic("Self increment only works on numeric or pointer types");
  }

  LLVMValueRef updated;
  if (t == TOK_SYM_SELF_INC) {
    // Calculate the new value with increment
    if (g_is_int_family_tok(base_type->ctype.type)) {
      updated = LLVMBuildAdd(b->builder, old, one, "inc1");
    } else if (base_type->ctype.type == '*') {
      // pointer increment
      slist pointer_type_points_to_type_chain =
          build_type_get_points_to_type_chian(b, points_to_type_chain);
      LLVMTypeRef pointer_points_to_base_type =
          build_type_ctype_convert_to_llvm(
              b, slist_peek_head(pointer_type_points_to_type_chain));

      updated = LLVMBuildGEP2(b->builder, pointer_points_to_base_type, old,
                              &one, 1, "ptrinc");
    } else {
      assert(g_is_fp_family_tok(base_type->ctype.type));
      updated = LLVMBuildFAdd(b->builder, old, one, "finc1");
    }
  } else {
    assert(t == TOK_SYM_SELF_DEC);
    // Calculate the new value with decrement
    if (g_is_int_family_tok(base_type->ctype.type)) {
      updated = LLVMBuildSub(b->builder, old, one, "dec1");
    } else if (base_type->ctype.type == '*') {
      // pointer decrement
      slist pointer_type_points_to_type_chain =
          build_type_get_points_to_type_chian(b, points_to_type_chain);
      LLVMTypeRef pointer_points_to_base_type =
          build_type_ctype_convert_to_llvm(
              b, slist_peek_head(pointer_type_points_to_type_chain));
      one = LLVMBuildNeg(b->builder, one, "negptrinc");
      updated = LLVMBuildGEP2(b->builder, pointer_points_to_base_type, old,
                              &one, 1, "ptrdec");
    } else {
      assert(g_is_fp_family_tok(base_type->ctype.type));
      updated = LLVMBuildFSub(b->builder, old, one, "fdec1");
    }
  }
  typed_value updated_v = typed_value_new(updated, points_to_type_chain);
  // Store the new value
  build_value_store(b, updated_v, expr);

  // For postfix, return the original value; for prefix, return the incremented value
  if (postfix) {
    return typed_value_new(old, points_to_type_chain);
  } else {
    return updated_v;
  }
}

typed_value build_expr_unary_func_call(builder b, astn n) {
  // function call
  typed_value func_expr = build_expression(b, n->unary.expr);
  // we could not use n and it's series API because the expr may be a temporary value
  slist func_return_type_chain = build_type_chain_copy(&func_expr->type_chain);
  astn func_base_type = slist_pop_head(func_return_type_chain);
  astn func_params = slist_pop_head(func_return_type_chain);
  if (func_base_type->ctype.type != '*' ||
      func_params->type != ast_parameters) {
    log_panic("this expression is not callable");
  }
  LLVMTypeRef ret_type = build_type_ctype_convert_to_llvm(
      b, slist_peek_head(func_return_type_chain));
  astn first_param = slist_peek_head(&func_params->parameters.list);
  astn last_param = slist_peek_tail(&func_params->parameters.list);
  LLVMTypeRef func_type;
  if (g_is_void_param(first_param)) {
    func_type = LLVMFunctionType(ret_type, NULL, 0, 0);
  } else {
    bool is_va = false;
    if (g_is_varargs_param(last_param)) {
      is_va = true;
    }
    struct dynarray params_type;
    dynarray_default(&params_type, sizeof(LLVMTypeRef));
    build_function_parameters_type(b, func_params, &params_type);
    func_type =
        LLVMFunctionType(ret_type, params_type.data, params_type.used, is_va);
    dynarray_free(&params_type);
  }
  // create args list
  size_t arg_idx = 1;
  struct dynarray args;
  dynarray_default(&args, sizeof(LLVMValueRef));
  astn arg;
  slist_foreach(&n->unary.extdata->arguments.list, arg) {
    astn corresponsed_param =
        slist_get(&func_params->parameters.list, arg_idx)->data;
    typed_value arg_expr = build_expression(b, arg);
    typed_value arg_casted = build_type_convert_by_type_chain(
        b, arg_expr, &corresponsed_param->declaration.type_chain);
    dynarray_add(&args, &arg_casted->v);
    arg_idx += 1;
  }
  astn func_return_base_type = slist_peek_head(func_return_type_chain);
  LLVMValueRef call = LLVMBuildCall2(
      b->builder, func_type, func_expr->v, args.data, args.used,
      func_return_base_type->ctype.type != TOK_KW_VOID ? "call_result" : "");

  dynarray_free(&args);
  return typed_value_new(call, func_return_type_chain);
}

typed_value build_expr_unary_get_member_ptr(builder b, astn n) {
  typed_value struct_or_union_ptr = build_lvalue_expression(b, n->unary.expr);
  astn member = n->unary.extdata;
  assert(member->type == ast_ident);
  slist points_to_struct_type_chain =
      build_type_get_points_to_type_chian(b, &struct_or_union_ptr->type_chain);
  astn points_to_base_type = slist_peek_head(points_to_struct_type_chain);
  assert(points_to_base_type->type == ast_ctype);
  if (!g_is_struct_or_union_token(points_to_base_type->ctype.type)) {
    log_panic("Only struct or union type can be used for . operation");
  }
  astn member_declaration;
  int member_idx = build_type_struct_type_get_member(
      points_to_base_type, member->ident, &member_declaration);
  if (member_idx < 0) {
    log_panic("Member %s not found in struct or union", member->ident);
  }
  slist member_type_chain = &member_declaration->declaration.type_chain;

  if (points_to_base_type->ctype.type == TOK_KW_STRUCT) {
    LLVMValueRef gep = LLVMBuildStructGEP2(
        b->builder, build_declaration_struct_or_union(b, points_to_base_type),
        struct_or_union_ptr->v, member_idx, "struct_gep");
    // add pointer to member type chain
    slist member_ptr_type_chain =
        build_type_chain_add_pointer(b, member_type_chain);
    // because gep computed the address of the member, we need to load it
    return typed_value_new(gep, member_ptr_type_chain);
  } else {
    // get union member: just return ptr directly and modify the type_chain
    slist member_ptr_type_chain =
        build_type_chain_add_pointer(b, member_type_chain);
    return typed_value_new(struct_or_union_ptr->v, member_ptr_type_chain);
  }
}

typed_value build_expr_unary_get_member(builder b, astn n) {
  return build_value_load(b, build_expr_unary_get_member_ptr(b, n));
}
typed_value build_expr_unary(builder b, astn n) {
  if (!n->unary.postfix) {
    // suffix
    switch (n->unary.op) {
    case '+':
      return build_expr_unary_pos(b, n->unary.expr);
    case '-':
      return build_expr_unary_neg(b, n->unary.expr);
    case '!':
      return build_expr_unary_not(b, n->unary.expr);
    case '~':
      return build_expr_unary_bit_not(b, n->unary.expr);
    case '*':
      return build_expr_unary_deref(b, n->unary.expr);
    case '&':
      return build_lvalue_expression(b, n->unary.expr);
    case TOK_SYM_SELF_INC:
    case TOK_SYM_SELF_DEC:
      return build_expr_unary_self_inc(b, n->unary.expr, n->unary.op,
                                       n->unary.postfix);
    case TOK_KW_SIZEOF:
      break;
    }
  } else {
    switch (n->unary.op) {
    case TOK_SYM_SELF_INC:
    case TOK_SYM_SELF_DEC:
      return build_expr_unary_self_inc(b, n->unary.expr, n->unary.op,
                                       n->unary.postfix);
    case '[': {
      // convert arr[idx] -> *(arr + idx)
      astn arr = n->unary.expr;
      astn idx = n->unary.extdata;
      // create binop
      astn binop = ast_new(ast_expr_binop);
      binop->binop.op = '+';
      binop->binop.lhs = arr;
      binop->binop.rhs = idx;
      typed_value r = build_expr_unary_deref(b, binop);
      binop->binop.lhs = binop->binop.rhs = NULL;
      ast_free(binop);
      return r;
    }
    case '(': {
      return build_expr_unary_func_call(b, n);
    }
    case TOK_SYM_ARROW: {
      // s->m is equivalent to (*s).m
      astn deref = ast_new(ast_expr_unary);
      deref->unary.op = '*';
      deref->unary.postfix = false;
      deref->unary.expr = n->unary.expr;
      n->unary.expr = deref;
      log_trace("replaced -> with .");
      n->unary.op = '.';
      typed_value result = build_expr_unary_get_member(b, n);
      n->unary.expr = deref->unary.expr;
      deref->unary.expr = NULL;
      ast_free(deref);
      return result;
    }

    case '.': {
      return build_expr_unary_get_member(b, n);
    }
    }
  }
  log_panic("Unsupported unary operation:%s", convert_repr_token(n->unary.op));
}

typed_value build_expr_primary(builder b, astn n) {
  switch (n->primary.type) {
  case TOK_LIT_INT: {
    return typed_value_new(LLVMConstInt(LLVMInt32TypeInContext(b->context),
                                        n->primary.v._int, true),
                           build_type_chain_expr_primary(n));
  }
  case TOK_LIT_UINT: {
    return typed_value_new(LLVMConstInt(LLVMInt32TypeInContext(b->context),
                                        n->primary.v._uint, false),
                           build_type_chain_expr_primary(n));
  }
  case TOK_LIT_LONG: {
    return typed_value_new(LLVMConstInt(LLVMInt64TypeInContext(b->context),
                                        n->primary.v._int, true),
                           build_type_chain_expr_primary(n));
  }
  case TOK_LIT_ULONG: {
    return typed_value_new(LLVMConstInt(LLVMInt64TypeInContext(b->context),
                                        n->primary.v._int, false),
                           build_type_chain_expr_primary(n));
  }
  case TOK_LIT_FLOAT:
    return typed_value_new(
        LLVMConstReal(LLVMFloatTypeInContext(b->context), n->primary.v._float),
        build_type_chain_expr_primary(n));
  case TOK_LIT_DOUBLE:
    return typed_value_new(LLVMConstReal(LLVMDoubleTypeInContext(b->context),
                                         n->primary.v._double),
                           build_type_chain_expr_primary(n));
  case TOK_LIT_CHAR:
    return typed_value_new(LLVMConstInt(LLVMInt8TypeInContext(b->context),
                                        n->primary.v._char, true),
                           build_type_chain_expr_primary(n));
  case TOK_LIT_STRING:
    BUILDING();
  default:
    log_panic("Unexpected literal token");
  }
}

typed_value build_load_declaration(builder b, astn n) {
  assert(n->type == ast_declaration);
  auto var = build_declaration(b, n);
  slist points_to_type_chain =
      build_type_get_points_to_type_chian(b, &var->type_chain);
  astn points_to_base_type = slist_peek_head(points_to_type_chain);
  if (points_to_base_type->type == ast_parameters) {
    // try to load function declaration
    // the function symbol itself is a pointer
    return typed_value_new(var->v, &var->type_chain);
  }
  return build_value_load(b, var);
}

typed_value build_expr_enum(builder b, astn n) {
  assert(n->type == ast_enumerator);
  return typed_value_new(LLVMConstInt(LLVMInt32TypeInContext(b->context),
                                      n->enumerator.value, false),
                         build_type_chain_by_lit(TOK_LIT_INT));
}

typed_value build_expr_ref(builder b, astn n) {
  switch (n->ref->type) {
  case ast_declaration:
    return build_load_declaration(b, n->ref);
  case ast_enumerator:
    return build_expr_enum(b, n->ref);
  default:
    log_panic("Unsupported ref type:%s", convert_repr_ast_type(n->ref->type));
  }
}

/**
 * @brief relocate the extern declaration, 
 * 
 * @param b 
 * @param n 
 * @return typed_value 
 */
typed_value build_relocate_declaration(builder b, astn n) {
  assert(n->type == ast_declaration);
  if (n->declaration.storage_class == TOK_KW_EXTERN) {
    // relocate extern declaration
    astn exist = parser_symtab_find(b->symtab, parse_declaration_get_ident(n));
    assert(exist);
    n = exist;
    log_trace("relocate extern declaration %s", n->declaration.ident);
  }

  return build_declaration(b, n);
}

typed_value build_expr_typecast(builder b, astn n) {
  slist type_chain = &n->typecast.type_chain;
  typed_value v = build_expression(b, n->typecast.expr);
  return build_type_convert_by_type_chain(b, v, type_chain);
}

typed_value build_expression(builder b, astn n) {
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
  case ast_ref: {
    return build_expr_ref(b, n);
  }
  case ast_expr_ternary: {
    return build_expr_ternary(b, n);
  }
  case ast_expr_typecast: {
    return build_expr_typecast(b, n);
  }
  default:
  }
  log_panic("Unsupported expression type:%s", convert_repr_ast_type(n->type));
}

typed_value build_lvalue_expression(builder b, astn n) {
  switch (n->type) {
  case ast_ref: {
    assert(n->ref->type == ast_declaration);
    return build_relocate_declaration(b, n->ref);
  }
  case ast_expr_unary: {
    switch (n->unary.op) {
    case '*':
      return build_expression(b, n->unary.expr);
    case '[': {
      // build lval arr[idx] -> *( arr + idx)
      astn arr = n->unary.expr;
      astn idx = n->unary.extdata;
      // create binop
      astn binop = ast_new(ast_expr_binop);
      binop->binop.op = '+';
      binop->binop.lhs = arr;
      binop->binop.rhs = idx;
      typed_value v = build_expr_binop(b, binop);
      binop->binop.lhs = binop->binop.rhs = NULL;
      ast_free(binop);
      return v;
    }
    case TOK_SYM_ARROW: {
      // s->m is equivalent to (*s).m
      astn deref = ast_new(ast_expr_unary);
      deref->unary.op = '*';
      deref->unary.postfix = false;
      deref->unary.expr = n->unary.expr;
      n->unary.expr = deref;
      log_trace("replaced -> with .");
      n->unary.op = '.';
      typed_value result = build_expr_unary_get_member_ptr(b, n);
      n->unary.expr = deref->unary.expr;
      deref->unary.expr = NULL;
      ast_free(deref);
      return result;
    }
    case '.': {
      return build_expr_unary_get_member_ptr(b, n);
    }
    }
    break;
  }
  default:
  }
  log_panic("lvalue expression expected");
}

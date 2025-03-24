#include "ast.h"
#include "builder.h"
#include "convert/convert.h"
#include "grammar.h"
#include "log/log.h"
#include "token.h"
#include "typed_value/typed_value.h"
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>

LLVMValueRef build_value_cmp0(builder b, typed_value v, LLVMIntPredicate iPred,
                              LLVMRealPredicate fPred, const char *label) {
  astn base_type = slist_peek_head(&v->type_chain);
  if (g_is_int_family_tok(base_type->ctype.type)) {
    return LLVMBuildICmp(
        b->builder, iPred, v->v,
        LLVMConstInt(build_type_ctype_convert_to_llvm(b, base_type), 0, false),
        label);
  } else if (base_type->ctype.type == '*') {
    return LLVMBuildICmp(
        b->builder, iPred, v->v,
        LLVMConstPointerNull(build_type_ctype_convert_to_llvm(b, base_type)),
        label);
  } else if (g_is_fp_family_tok(base_type->ctype.type)) {
    return LLVMBuildFCmp(
        b->builder, fPred, v->v,
        LLVMConstReal(build_type_ctype_convert_to_llvm(b, base_type), 0),
        label);
  }
  log_panic("Unsupported type comparison: %s",
            convert_repr_ast_type(base_type->ctype.type));
}

LLVMValueRef build_value_eq0(builder b, typed_value v) {
  return build_value_cmp0(b, v, LLVMIntEQ, LLVMRealOEQ, "test0");
}

LLVMValueRef build_value_ne0(builder b, typed_value v) {
  return build_value_cmp0(b, v, LLVMIntNE, LLVMRealONE, "testnot0");
}

void build_value_store(builder b, typed_value v, typed_value ptr) {
  // get lhs points type
  slist points_to_type_chain =
      build_type_get_points_to_type_chian(b, &ptr->type_chain);
  // cast rhs to lhs type
  v = build_type_convert_by_type_chain(b, v, points_to_type_chain);
  // store rhs to lhs
  LLVMBuildStore(b->builder, v->v, ptr->v);
}

typed_value build_value_load(builder b, typed_value v) {
  // get lhs points type
  slist points_to_type_chain =
      build_type_get_points_to_type_chian(b, &v->type_chain);
  astn points_to_base_type = slist_peek_head(points_to_type_chain);
  LLVMTypeRef points_to_type =
      build_type_ctype_convert_to_llvm(b, points_to_base_type);
  LLVMValueRef load;
  if (points_to_base_type->type == ast_ctype &&
      g_is_struct_or_union_token(points_to_base_type->ctype.type)) {
    log_trace("Try to load struct or union type from pointer, return the "
              "struct pointer "
              "directly");
    load = v->v;
  } else {
    load = LLVMBuildLoad2(b->builder, points_to_type, v->v, "load");
  }
  return typed_value_new(load, points_to_type_chain);
}

typed_value build_value_expr_binop_template(builder b, typed_value lhs,
                                            typed_value rhs,
                                            llvm_func_t llvm_build_f[2],
                                            char *f_names[2]) {
  if (build_expr_is_binop_with_ptr(lhs, rhs)) {
    log_panic("Ptr should not be used in there, it should be processed in "
              "build_expr_binop");
  }
  typed_value *exprs =
      build_type_2_values_type_upper_cast(b, (typed_value[]){lhs, rhs});
  astn base_type = slist_peek_head(&exprs[0]->type_chain);
  if (g_is_int_family_tok(base_type->ctype.type)) {
    return typed_value_new(
        llvm_build_f[0](b->builder, exprs[0]->v, exprs[1]->v, f_names[0]),
        &exprs[0]->type_chain);
  } else if (g_is_fp_family_tok(base_type->ctype.type)) {
    return typed_value_new(
        llvm_build_f[1](b->builder, exprs[0]->v, exprs[1]->v, f_names[1]),
        &exprs[0]->type_chain);
  }
  log_panic("Unsupported type in binary operation:%s",
            convert_repr_ast_type(base_type->ctype.type));
}

typed_value build_value_expr_binop_div(builder b, typed_value lhs,
                                       typed_value rhs) {
  if (build_expr_is_binop_with_ptr(lhs, rhs)) {
    log_panic("Ptr should not be used in there, it should be processed in "
              "build_expr_binop");
  }
  typed_value *exprs =
      build_type_2_values_type_upper_cast(b, (typed_value[]){lhs, rhs});
  astn base_type = slist_peek_head(&exprs[0]->type_chain);
  if (g_is_int_family_tok(base_type->ctype.type) &&
      base_type->ctype.signint == TOK_KW_SIGNED) {
    return typed_value_new(
        LLVMBuildSDiv(b->builder, exprs[0]->v, exprs[1]->v, "sdiv"),
        &exprs[0]->type_chain);
  } else if (g_is_int_family_tok(base_type->ctype.type) &&
             base_type->ctype.signint == TOK_KW_UNSIGNED) {
    return typed_value_new(
        LLVMBuildUDiv(b->builder, exprs[0]->v, exprs[1]->v, "udiv"),
        &exprs[0]->type_chain);
  } else if (g_is_fp_family_tok(base_type->ctype.type)) {
    return typed_value_new(
        LLVMBuildFDiv(b->builder, exprs[0]->v, exprs[1]->v, "fdiv"),
        &exprs[0]->type_chain);
  } else {
    log_panic("Unsupported type in div operation:%s",
              convert_repr_ast_type(base_type->ctype.type));
  }
}

typed_value build_value_expr_binop_su_template(builder b, typed_value lhs,
                                               typed_value rhs,
                                               llvm_func_t llvm_build_f[2],
                                               char *f_names[2]) {
  if (build_expr_is_binop_with_ptr(lhs, rhs)) {
    log_panic("Ptr should not be used in this binop, it should be processed in "
              "build_expr_binop");
  }
  typed_value *exprs =
      build_type_2_values_type_upper_cast(b, (typed_value[]){lhs, rhs});

  // only int family is allowed
  astn base_type = slist_peek_head(&exprs[0]->type_chain);
  if (!g_is_int_family_tok(base_type->ctype.type)) {
    log_panic("%s or %s operation only allowed on int type", f_names[0],
              f_names[1]);
  }

  if (base_type->ctype.signint == TOK_KW_SIGNED) {
    return typed_value_new(
        llvm_build_f[0](b->builder, exprs[0]->v, exprs[1]->v, f_names[0]),
        &exprs[0]->type_chain);
  }
  // unsigned
  return typed_value_new(
      llvm_build_f[1](b->builder, exprs[0]->v, exprs[1]->v, f_names[1]),
      &exprs[0]->type_chain);
}

typed_value build_value_expr_binop_bit_template(builder b, typed_value lhs,
                                                typed_value rhs,
                                                llvm_func_t llvm_build_f,
                                                char *f_name) {
  if (build_expr_is_binop_with_ptr(lhs, rhs)) {
    log_panic("Ptr should not be used in there, it should be processed in "
              "build_expr_binop");
  }
  typed_value *exprs =
      build_type_2_values_type_upper_cast(b, (typed_value[]){lhs, rhs});
  // only int family is allowed
  astn lhs_base_type = slist_peek_head(&exprs[0]->type_chain);
  astn rhs_base_type = slist_peek_head(&exprs[1]->type_chain);
  if (!(g_is_int_family_tok(lhs_base_type->ctype.type) &&
        g_is_int_family_tok(rhs_base_type->ctype.type))) {
    log_panic("%s operation is only allowed on int type", f_name);
  }
  return typed_value_new(
      llvm_build_f(b->builder, exprs[0]->v, exprs[1]->v, f_name),
      &exprs[0]->type_chain);
}
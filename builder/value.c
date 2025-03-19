#include "builder.h"
#include "convert/convert.h"
#include "grammar.h"
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>

LLVMValueRef build_value_cmp0(builder b, typed_value v, LLVMIntPredicate iPred,
                              LLVMRealPredicate fPred, const char *label) {
  astn base_type = slist_peek_head(&v->type_chain);
  if (g_is_int_family_tok(base_type->ctype.type)) {
    return LLVMBuildICmp(
        b->builder, iPred, v->v,
        LLVMConstInt(build_convert_base_type(b, base_type), 0, false), label);
  } else if (base_type->ctype.type == '*') {
    return LLVMBuildICmp(
        b->builder, iPred, v->v,
        LLVMConstPointerNull(build_convert_base_type(b, base_type)), label);
  } else if (g_is_fp_family_tok(base_type->ctype.type)) {
    return LLVMBuildFCmp(
        b->builder, fPred, v->v,
        LLVMConstReal(build_convert_base_type(b, base_type), 0), label);
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
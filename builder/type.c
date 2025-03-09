#include "builder.h"
#include "log/log.h"
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
llvm_typed_value *
build_2_values_type_upper_cast(builder b, llvm_typed_value *values) {
  LLVMValueRef lhs = values[0]->v;
  LLVMValueRef rhs = values[1]->v;
  LLVMTypeRef lhs_ty = LLVMTypeOf(lhs);
  LLVMTypeRef rhs_ty = LLVMTypeOf(rhs);
  if (lhs_ty == rhs_ty) {
    return values;
  } else if (LLVMGetTypeKind(lhs_ty) == LLVMIntegerTypeKind &&
             LLVMGetTypeKind(rhs_ty) == LLVMIntegerTypeKind) {
    // Get bit widths
    unsigned lhs_width = LLVMGetIntTypeWidth(lhs_ty);
    unsigned rhs_width = LLVMGetIntTypeWidth(rhs_ty);

    // Cast the smaller integer type to the larger one
    if (lhs_width < rhs_width) {
      // Cast lhs to rhs_ty
      values[0]->v = LLVMBuildSExt(b->builder, lhs, rhs_ty, "cast");
      values[0]->type_chain = values[1]->type_chain;
    } else {
      // Cast rhs to lhs_ty
      values[1]->v = LLVMBuildSExt(b->builder, rhs, lhs_ty, "cast");
      values[1]->type_chain = values[0]->type_chain;
    }
    return values;
  } else if (LLVMGetTypeKind(lhs_ty) == LLVMFloatTypeKind ||
             LLVMGetTypeKind(lhs_ty) == LLVMDoubleTypeKind ||
             LLVMGetTypeKind(rhs_ty) == LLVMFloatTypeKind ||
             LLVMGetTypeKind(rhs_ty) == LLVMDoubleTypeKind) {
    // If one is float and the other is double, cast to double
    if (LLVMGetTypeKind(lhs_ty) == LLVMDoubleTypeKind &&
        LLVMGetTypeKind(rhs_ty) == LLVMFloatTypeKind) {
      values[1]->v = LLVMBuildFPExt(b->builder, rhs, lhs_ty, "cast");
      values[1]->type_chain = values[0]->type_chain;
    } else if (LLVMGetTypeKind(rhs_ty) == LLVMDoubleTypeKind &&
               LLVMGetTypeKind(lhs_ty) == LLVMFloatTypeKind) {
      values[0]->v = LLVMBuildFPExt(b->builder, lhs, rhs_ty, "cast");
      values[0]->type_chain = values[1]->type_chain;
    }
    return values;
  } else if (LLVMGetTypeKind(lhs_ty) == LLVMIntegerTypeKind &&
             (LLVMGetTypeKind(rhs_ty) == LLVMFloatTypeKind ||
              LLVMGetTypeKind(rhs_ty) == LLVMDoubleTypeKind)) {
    // Handle mixed integer and floating-point types
    values[0]->v = LLVMBuildSIToFP(b->builder, lhs, rhs_ty, "cast");
    values[0]->type_chain = values[1]->type_chain;
    return values;
  } else if (LLVMGetTypeKind(rhs_ty) == LLVMIntegerTypeKind &&
             (LLVMGetTypeKind(lhs_ty) == LLVMFloatTypeKind ||
              LLVMGetTypeKind(lhs_ty) == LLVMDoubleTypeKind)) {
    // same as above
    values[1]->v = LLVMBuildSIToFP(b->builder, rhs, lhs_ty, "cast");
    values[1]->type_chain = values[0]->type_chain;
    return values;
  }
  log_error("Invalid type cast");
  return values;
}

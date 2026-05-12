
#include "dcc.h"
#include <assert.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static LLVMContextRef C;
static LLVMModuleRef M;
static LLVMBuilderRef B;
static LLVMValueRef F;

static HashMap func_labels;

static LLVMValueRef llvm_memset_declare;
static LLVMTypeRef llvm_memset_declare_ty;

static LLVMValueRef gen_expr(Node *node);
static LLVMValueRef gen_stmt(Node *node);

static LLVMTypeRef type_convert(Type *ty) {
  switch (ty->kind) {
  case TY_VOID:
    return LLVMVoidTypeInContext(C);
  case TY_BOOL:
    return LLVMInt8TypeInContext(C);
  case TY_CHAR:
    assert(ty->size == sizeof(char));
    return LLVMInt8TypeInContext(C);
  case TY_SHORT:
    return LLVMInt16TypeInContext(C);
  case TY_INT:
    return LLVMInt32TypeInContext(C);
  case TY_LONG:
    return LLVMInt64TypeInContext(C);
  case TY_FLOAT:
    return LLVMFloatTypeInContext(C);
  case TY_DOUBLE:
    return LLVMDoubleTypeInContext(C);
  case TY_LDOUBLE:
    return LLVMFP128TypeInContext(C);
  case TY_PTR:
    return LLVMPointerTypeInContext(C, 0);
  case TY_ARRAY:
    assert(ty->array_len > 0);
    return LLVMArrayType2(type_convert(ty->base), ty->array_len);
  case TY_STRUCT: {
    size_t member_count = next_iter_count(ty->members);
    LLVMTypeRef *members_type = calloc(member_count, sizeof(LLVMTypeRef));
    {
      size_t members_type_idx = 0;
      for (Member *m = ty->members; m; m = m->next) {
        members_type[members_type_idx++] = type_convert(m->ty);
      }
      assert(member_count == members_type_idx);
    }
    LLVMTypeRef r =
        LLVMStructTypeInContext(C, members_type, member_count, ty->is_packed);
    free(members_type);
    return r;
  }
  case TY_FUNC: {
    size_t param_count = next_iter_count(ty->params);
    LLVMTypeRef *params = calloc(param_count, sizeof(LLVMTypeRef));
    {
      size_t params_idx = 0;
      for (Type *p = ty->params; p; p = p->next) {
        params[params_idx++] = type_convert(p);
      }
      assert(params_idx == param_count);
    }
    LLVMTypeRef r = LLVMFunctionType(type_convert(ty->return_ty), params,
                                     param_count, ty->is_variadic);
    free(params);
    return r;
  }

  case TY_ENUM:
  case TY_VLA:
  case TY_UNION:
  default:
    break;
  }
  unreachable();
}

static LLVMValueRef init_global_data(Type *ty, Initializer *init) {
  LLVMTypeRef llvm_ty = type_convert(ty);
  LLVMValueRef init_val;
  if (ty->kind == TY_ARRAY) {
    LLVMValueRef *cv_array = calloc(ty->array_len, sizeof(LLVMValueRef));
    for (int i = 0; i < ty->array_len; i++) {
      cv_array[i] = init_global_data(ty->base, init->children[i]);
    }
    init_val = LLVMConstArray2(type_convert(ty->base), cv_array, ty->array_len);
    free(cv_array);
  } else if (ty->kind == TY_UNION) {
    todo_impl("union");
  } else if (ty->kind == TY_STRUCT) {
    size_t member_count = next_iter_count(ty->members);
    LLVMValueRef *cv_array = calloc(ty->array_len, sizeof(LLVMValueRef));
    {
      size_t cv_array_idx = 0;
      for (Member *m = ty->members; m; m = m->next) {
        if (m->is_bitfield) {
          todo_impl("bitfield global init");
        }
        cv_array[cv_array_idx++] =
            init_global_data(m->ty, init->children[m->idx]);
      }
      assert(cv_array_idx == member_count);
    }
    init_val = LLVMConstNamedStruct(type_convert(ty), cv_array, member_count);
    free(cv_array);
  } else if (ty->kind == TY_DOUBLE || ty->kind == TY_FLOAT) {
    init_val = LLVMConstReal(llvm_ty, eval_double(init->expr));
  } else if (!init->expr) {
    init_val = LLVMConstNull(llvm_ty);
  } else {
    // integer family and ptr
    char **var_ref = NULL;
    int64_t eval_val = eval2(init->expr, &var_ref);
    if (var_ref) {
      // Relocation pointer, real data: label + eval_val
      LLVMValueRef target_val = LLVMGetNamedGlobal(M, *var_ref);
      if (!target_val) {
        // maybe it's a function
        target_val = LLVMGetNamedFunction(M, *var_ref);
      }
      assert(target_val);
      assert(ty->base);
      LLVMTypeRef pointee_ty = type_convert(ty->base);
      LLVMValueRef indices =
          LLVMConstInt(LLVMInt64TypeInContext(C), (uint64_t)eval_val, false);
      init_val = LLVMConstInBoundsGEP2(pointee_ty, target_val, &indices, 1);
    } else {
      // int family
      init_val = LLVMConstInt(llvm_ty, eval_val, ty->is_unsigned);
    }
  }
  return init_val;
}

// The first stage. Only declare global variable to avoid dependency order
// problem.
static void codegen_global_declare(Obj *prog) {
  for (Obj *var = prog; var; var = var->next) {
    LLVMValueRef v;
    if (var->is_function) {
      LLVMTypeRef fn_ty = type_convert(var->ty);
      v = LLVMAddFunction(M, var->name, fn_ty);
      if (var->is_inline) {
        LLVMAttributeRef inline_attr =
            LLVMCreateStringAttribute(C, "inlinehint", 10, "", 0);
        LLVMAddAttributeAtIndex(v, LLVMAttributeFunctionIndex, inline_attr);
      }
    } else {
      LLVMTypeRef ty = type_convert(var->ty);
      v = LLVMAddGlobal(M, ty, var->name);
    }

    if (var->is_static) {
      LLVMSetLinkage(v, LLVMInternalLinkage);
    }

    if (!var->is_definition) {
      LLVMSetLinkage(v, LLVMExternalLinkage);
    }

    if (var->is_tentative) {
      LLVMSetLinkage(v, LLVMCommonLinkage);
    }

    if (var->is_tls) {
      LLVMSetThreadLocal(v, true);
    }

    if (!var->is_function) {
      LLVMSetAlignment(v, var->ty->align);
    }
    var->codegen_data = (intptr_t)v;
  }
}

static void new_block(char *name) {
  LLVMBasicBlockRef blk_name = LLVMAppendBasicBlockInContext(C, F, name);
  LLVMBuildBr(B, blk_name);
  LLVMPositionBuilderAtEnd(B, blk_name);
}

enum { I8, I16, I32, I64, U8, U16, U32, U64, F32, F64, F128, PTR };

static int getTypeId(Type *ty) {
  switch (ty->kind) {
  case TY_BOOL:
    return I8;
  case TY_CHAR:
    return ty->is_unsigned ? U8 : I8;
  case TY_SHORT:
    return ty->is_unsigned ? U16 : I16;
  case TY_INT:
    return ty->is_unsigned ? U32 : I32;
  case TY_LONG:
    return ty->is_unsigned ? U64 : I64;
  case TY_FLOAT:
    return F32;
  case TY_DOUBLE:
    return F64;
  case TY_LDOUBLE:
    return F128;
  case TY_FUNC:
  case TY_ARRAY:
  case TY_PTR:
    return PTR;
  case TY_VLA:
  case TY_STRUCT:
  case TY_UNION:
    break;
  }
  unreachable();
}
enum { CAST_NOP = -1, CAST_INVALID = -2 };

static int cast_table[12][12] = {
    [I8] =
        {
            [I8] = CAST_NOP,
            [I16] = LLVMSExt,
            [I32] = LLVMSExt,
            [I64] = LLVMSExt,
            [U8] = CAST_NOP,
            [U16] = LLVMZExt,
            [U32] = LLVMZExt,
            [U64] = LLVMZExt,
            [F32] = LLVMSIToFP,
            [F64] = LLVMSIToFP,
            [F128] = LLVMSIToFP,
            [PTR] = LLVMIntToPtr,
        },
    [I16] =
        {
            [I8] = LLVMTrunc,
            [I16] = CAST_NOP,
            [I32] = LLVMSExt,
            [I64] = LLVMSExt,
            [PTR] = LLVMIntToPtr,
            [U8] = LLVMTrunc,
            [U16] = CAST_NOP,
            [U32] = LLVMZExt,
            [U64] = LLVMZExt,
            [F32] = LLVMSIToFP,
            [F64] = LLVMSIToFP,
            [F128] = LLVMSIToFP,
        },
    [I32] =
        {
            [I8] = LLVMTrunc,
            [I16] = LLVMTrunc,
            [I32] = CAST_NOP,
            [I64] = LLVMSExt,
            [PTR] = LLVMIntToPtr,
            [U8] = LLVMTrunc,
            [U16] = LLVMTrunc,
            [U32] = CAST_NOP,
            [U64] = LLVMZExt,
            [F32] = LLVMSIToFP,
            [F64] = LLVMSIToFP,
            [F128] = LLVMSIToFP,
        },
    [I64] =
        {
            [I8] = LLVMTrunc,
            [I16] = LLVMTrunc,
            [I32] = LLVMTrunc,
            [I64] = CAST_NOP,
            [PTR] = LLVMIntToPtr,
            [U8] = LLVMTrunc,
            [U16] = LLVMTrunc,
            [U32] = LLVMTrunc,
            [U64] = CAST_NOP,
            [F32] = LLVMSIToFP,
            [F64] = LLVMSIToFP,
            [F128] = LLVMSIToFP,
        },
    [U8] =
        {
            [I8] = CAST_NOP,
            [I16] = LLVMZExt,
            [I32] = LLVMZExt,
            [I64] = LLVMZExt,
            [PTR] = LLVMIntToPtr,
            [U8] = CAST_NOP,
            [U16] = LLVMZExt,
            [U32] = LLVMZExt,
            [U64] = LLVMZExt,
            [F32] = LLVMUIToFP,
            [F64] = LLVMUIToFP,
            [F128] = LLVMUIToFP,
        },
    [U16] =
        {
            [I8] = LLVMTrunc,
            [I16] = CAST_NOP,
            [I32] = LLVMZExt,
            [I64] = LLVMZExt,
            [PTR] = LLVMIntToPtr,
            [U8] = LLVMTrunc,
            [U16] = CAST_NOP,
            [U32] = LLVMZExt,
            [U64] = LLVMZExt,
            [F32] = LLVMUIToFP,
            [F64] = LLVMUIToFP,
            [F128] = LLVMUIToFP,
        },
    [U32] =
        {
            [I8] = LLVMTrunc,
            [I16] = LLVMTrunc,
            [I32] = CAST_NOP,
            [I64] = LLVMZExt,
            [PTR] = LLVMIntToPtr,
            [U8] = LLVMTrunc,
            [U16] = LLVMTrunc,
            [U32] = CAST_NOP,
            [U64] = LLVMZExt,
            [F32] = LLVMUIToFP,
            [F64] = LLVMUIToFP,
            [F128] = LLVMUIToFP,
        },
    [U64] =
        {
            [I8] = LLVMTrunc,
            [I16] = LLVMTrunc,
            [I32] = LLVMTrunc,
            [I64] = CAST_NOP,
            [PTR] = LLVMIntToPtr,
            [U8] = LLVMTrunc,
            [U16] = LLVMTrunc,
            [U32] = LLVMTrunc,
            [U64] = CAST_NOP,
            [F32] = LLVMUIToFP,
            [F64] = LLVMUIToFP,
            [F128] = LLVMUIToFP,
        },
    [F32] =
        {
            [I8] = LLVMFPToSI,
            [I16] = LLVMFPToSI,
            [I32] = LLVMFPToSI,
            [I64] = LLVMFPToSI,
            [U8] = LLVMFPToUI,
            [U16] = LLVMFPToUI,
            [U32] = LLVMFPToUI,
            [U64] = LLVMFPToUI,
            [F32] = CAST_NOP,
            [F64] = LLVMFPExt,
            [F128] = LLVMFPExt,
            [PTR] = CAST_INVALID,
        },
    [F64] =
        {
            [I8] = LLVMFPToSI,
            [I16] = LLVMFPToSI,
            [I32] = LLVMFPToSI,
            [I64] = LLVMFPToSI,
            [U8] = LLVMFPToUI,
            [U16] = LLVMFPToUI,
            [U32] = LLVMFPToUI,
            [U64] = LLVMFPToUI,
            [F32] = LLVMFPTrunc,
            [F64] = CAST_NOP,
            [F128] = LLVMFPExt,
            [PTR] = CAST_INVALID,
        },
    [F128] =
        {
            [I8] = LLVMFPToSI,
            [I16] = LLVMFPToSI,
            [I32] = LLVMFPToSI,
            [I64] = LLVMFPToSI,
            [U8] = LLVMFPToUI,
            [U16] = LLVMFPToUI,
            [U32] = LLVMFPToUI,
            [U64] = LLVMFPToUI,
            [F32] = LLVMFPTrunc,
            [F64] = LLVMFPTrunc,
            [F128] = CAST_NOP,
            [PTR] = CAST_INVALID,
        },
    [PTR] =
        {
            [I8] = LLVMPtrToInt,
            [I16] = LLVMPtrToInt,
            [I32] = LLVMPtrToInt,
            [I64] = LLVMPtrToInt,
            [U8] = LLVMPtrToInt,
            [U16] = LLVMPtrToInt,
            [U32] = LLVMPtrToInt,
            [U64] = LLVMPtrToInt,
            [PTR] = LLVMBitCast,
            [F32] = CAST_INVALID,
            [F64] = CAST_INVALID,
            [F128] = CAST_INVALID,
        },
};

static LLVMValueRef cast(LLVMValueRef v, Type *from, Type *to, Token *tok) {
  if (to->kind == TY_VOID) {
    error_tok(tok, "It's not allowed convert type to void");
  }

  int from_id = getTypeId(from);
  int to_id = getTypeId(to);
  int op = cast_table[from_id][to_id];
  if (op == CAST_NOP) {
    return v;
  } else if (op == CAST_INVALID) {
    error_tok(tok, "Invalid type cast");
  }
  return LLVMBuildCast(B, (LLVMOpcode)op, v, type_convert(to), "cast");
}

static LLVMValueRef gen_addr(Node *node) {
  switch (node->kind) {
  case ND_VAR:
    // the variable ptr has been stored in codegen_data
    return (LLVMValueRef)node->var->codegen_data;
  case ND_DEREF:
    return gen_expr(node->lhs);
  case ND_COMMA:
    gen_expr(node->lhs);
    return gen_addr(node->rhs);
  case ND_MEMBER: {
    LLVMValueRef ptr = gen_addr(node->lhs);
    return LLVMBuildGEP2(
        B, type_convert(node->ty), ptr,
        &(LLVMValueRef){
            LLVMConstInt(LLVMInt64TypeInContext(C), node->member->idx, false)},
        1, "mem_GEP");
  }
  case ND_FUNCALL:
    if (node->ret_buffer) {
      todo_impl("return struct/union");
      return gen_expr(node);
    }
    break;
  case ND_ASSIGN:
  case ND_COND:
    if (node->ty->kind == TY_STRUCT || node->ty->kind == TY_UNION)
      return gen_expr(node);
    break;
  case ND_VLA_PTR:
    todo_impl("vla ptr");
  }

  error_tok(node->tok, "not an lvalue");
}

static LLVMValueRef load(Type *pointee_ty, LLVMValueRef ptr) {
  switch (pointee_ty->kind) {
  case TY_ARRAY:
  case TY_STRUCT:
  case TY_UNION:
  case TY_FUNC:
  case TY_VLA:
    // we can't load them so we just return the ptr
    return ptr;
  default:
    break;
  }
  return LLVMBuildLoad2(B, type_convert(pointee_ty), ptr, "load");
}

static void store(Type *ty, LLVMValueRef ptr, LLVMValueRef v) {
  switch (ty->kind) {
  case TY_STRUCT:
  case TY_UNION:
    todo_impl("store struct/union");
  default:
    break;
  }
  LLVMBuildStore(B, v, ptr);
}

static void llvm_memset2(LLVMValueRef ptr, LLVMValueRef byte, LLVMValueRef n,
                         LLVMValueRef immarg) {
  LLVMBuildCall2(B, llvm_memset_declare_ty, llvm_memset_declare,
                 (LLVMValueRef[4]){ptr, byte, n, immarg}, 4, "");
}

static void llvm_memset(LLVMValueRef ptr, char byte, size_t n,
                        bool is_volatile) {
  llvm_memset2(ptr, LLVMConstInt(LLVMInt8TypeInContext(C), byte, false),
               LLVMConstInt(LLVMInt64TypeInContext(C), n, false),
               LLVMConstInt(LLVMInt1TypeInContext(C), is_volatile, false));
}

// Return i1, so do not use it as _Bool(i8) type directly
static LLVMValueRef cmp_nz(LLVMValueRef v) {
  LLVMTypeRef vty = LLVMTypeOf(v);
  LLVMTypeKind vk = LLVMGetTypeKind(vty);
  LLVMValueRef zero = LLVMConstNull(vty);
  if (vk == LLVMIntegerTypeKind || vk == LLVMPointerTypeKind) {
    return LLVMBuildICmp(B, LLVMIntNE, v, zero, "cmp_nz_i");
  }
  return LLVMBuildFCmp(B, LLVMRealONE, v, zero, "cmp_nz_f");
}

// Return i1, so do not use it as _Bool(i8) type directly
static LLVMValueRef cmp_ez(LLVMValueRef v) {
  LLVMTypeRef vty = LLVMTypeOf(v);
  LLVMTypeKind vk = LLVMGetTypeKind(vty);
  LLVMValueRef zero = LLVMConstNull(vty);
  if (vk == LLVMIntegerTypeKind || vk == LLVMPointerTypeKind) {
    return LLVMBuildICmp(B, LLVMIntEQ, v, zero, "cmp_ez_i");
  }
  return LLVMBuildFCmp(B, LLVMRealOEQ, v, zero, "cmp_ez_f");
}

static LLVMValueRef logic_short_circuit(Node *node, bool is_and) {
  LLVMValueRef lhs = gen_expr(node->lhs);
  LLVMValueRef lhs_check = cmp_nz(lhs);
  LLVMBasicBlockRef start_block = LLVMGetInsertBlock(B);
  LLVMBasicBlockRef next_block =
      LLVMAppendBasicBlockInContext(C, F, is_and ? "and_next" : "or_next");
  LLVMBasicBlockRef merge_block =
      LLVMAppendBasicBlockInContext(C, F, is_and ? "and_merge" : "or_merge");

  if (is_and) {
    LLVMBuildCondBr(B, lhs_check, next_block, merge_block);
  } else {
    LLVMBuildCondBr(B, lhs_check, merge_block, next_block);
  }

  LLVMPositionBuilderAtEnd(B, next_block);
  LLVMValueRef rhs = gen_expr(node->rhs);
  LLVMValueRef rhs_check = cmp_nz(rhs);
  LLVMBuildBr(B, merge_block);

  LLVMPositionBuilderAtEnd(B, merge_block);
  LLVMValueRef phi =
      LLVMBuildPhi(B, LLVMInt1TypeInContext(C), is_and ? "and_phi" : "or_phi");
  LLVMAddIncoming(phi, (LLVMValueRef[]){lhs_check, rhs_check},
                  (LLVMBasicBlockRef[]){start_block, next_block}, 2);
  LLVMValueRef ext = LLVMBuildZExt(B, phi, LLVMInt32TypeInContext(C),
                                   is_and ? "zext_and" : "zext_or");
  return ext;
}

static LLVMValueRef gen_expr(Node *node) {
  switch (node->kind) {
  case ND_NULL_EXPR: {
    return NULL;
  }
  case ND_NUM: {
    switch (node->ty->kind) {
    case TY_FLOAT:
    case TY_DOUBLE:
    case TY_LDOUBLE:
      return LLVMConstReal(type_convert(node->ty), node->fval);
    default:
      return LLVMConstInt(type_convert(node->ty), node->val,
                          !node->ty->is_unsigned);
    }
  }
  case ND_NEG: {
    if (is_flonum(node->lhs->ty)) {
      return LLVMBuildFNeg(B, gen_expr(node->lhs), "fneg");
    } else {
      return LLVMBuildNeg(B, gen_expr(node->lhs), "neg");
    }
  }
  case ND_CAST: {
    return cast(gen_expr(node->lhs), node->lhs->ty, node->ty, node->tok);
  }
  case ND_VAR: {
    return load(node->ty, gen_addr(node));
  }
  case ND_DEREF: {
    return load(node->ty, gen_expr(node->lhs));
  }
  case ND_ADDR: {
    return gen_addr(node->lhs);
  }
  case ND_MEMBER: {
    return load(node->ty, gen_addr(node));
  }
  case ND_STMT_EXPR: {
    new_block("stmt_expr");
    LLVMValueRef r = NULL;
    // statement expression will return the last expression statement
    for (Node *n = node->body; n; n = n->next)
      r = gen_stmt(n);
    if (!r) {
      error_tok(node->tok, "This statement expression returns a invalid type");
    }
    return r;
  }
  case ND_COMMA:
    gen_expr(node->lhs);
    return gen_expr(node->rhs);
  case ND_ASSIGN: {
    LLVMValueRef ptr = gen_addr(node->lhs);
    LLVMValueRef v = gen_expr(node->rhs);
    if (node->lhs->kind == ND_MEMBER && node->lhs->member->is_bitfield) {
      todo_impl("assign bitfield");
    }
    store(node->ty, ptr, v);
    // load again
    return load(node->lhs->ty, ptr);
  }
  case ND_MEMZERO: {
    assert(node->var->codegen_data);
    llvm_memset((LLVMValueRef)node->var->codegen_data, 0, node->var->ty->size,
                false);
    return NULL;
  }
  case ND_COND: {
    LLVMValueRef cond = gen_expr(node->cond);
    cond = cmp_nz(cond);
    LLVMBasicBlockRef bb_then =
        LLVMAppendBasicBlockInContext(C, F, "cond_then");
    LLVMBasicBlockRef bb_else =
        LLVMAppendBasicBlockInContext(C, F, "cond_else");
    LLVMBasicBlockRef bb_merge =
        LLVMAppendBasicBlockInContext(C, F, "cond_merge");
    LLVMBuildCondBr(B, cond, bb_then, bb_else);
    LLVMPositionBuilderAtEnd(B, bb_then);
    LLVMValueRef then_v = gen_expr(node->then);
    if (!then_v) {
      error_tok(node->tok, "there isn't any value returned from true path");
    }
    LLVMBuildBr(B, bb_merge);
    LLVMPositionBuilderAtEnd(B, bb_else);
    LLVMValueRef else_v = gen_expr(node->_else);
    if (!else_v) {
      error_tok(node->tok, "there isn't any value returned from false path");
    }
    LLVMBuildBr(B, bb_merge);
    LLVMPositionBuilderAtEnd(B, bb_merge);
    LLVMValueRef phi =
        LLVMBuildPhi(B, type_convert(node->ty), "cond_merge_phi");
    LLVMAddIncoming(phi, (LLVMValueRef[]){then_v, else_v},
                    (LLVMBasicBlockRef[]){bb_then, bb_else}, 2);
    return phi;
  }
  case ND_BITNOT: {
    LLVMValueRef v = gen_expr(node->lhs);
    return LLVMBuildNot(B, v, "bitnot");
  }
  case ND_NOT: {
    LLVMValueRef v = gen_expr(node->lhs);
    v = cmp_ez(v);
    // we have to ext i1 to i32
    return LLVMBuildZExt(B, v, LLVMInt32TypeInContext(C), "not_ext");
  }
  case ND_LOGAND: {
    return logic_short_circuit(node, true);
  }
  case ND_LOGOR: {
    return logic_short_circuit(node, false);
  }
  case ND_FUNCALL: {
    // built-in alloca function
    if (node->lhs->kind == ND_VAR &&
        !strcmp(node->lhs->var->name, "__builtin_alloca")) {
      LLVMValueRef sz = gen_expr(node->args);
      return LLVMBuildArrayAlloca(B, LLVMInt8TypeInContext(C), sz,
                                  "__builtin_alloca");
    }

    LLVMValueRef F = gen_expr(node->lhs);

    if (node->ret_buffer) {
      todo_impl("return struct from funcall");
    }

    size_t arg_count = next_iter_count(node->args);
    LLVMValueRef *args = calloc(arg_count, sizeof(LLVMValueRef));
    size_t arg_idx = 0;
    for (Node *arg = node->args; arg; arg = arg->next) {
      args[arg_idx++] = gen_expr(arg);
    }
    Type *F_ty;
    if (node->lhs->ty->kind == TY_PTR) {
      // extract the function type from ponter because LLVM just can recognise
      // function this in call format
      F_ty = node->lhs->ty->base;
    } else {
      F_ty = node->lhs->ty;
    }
    LLVMValueRef r =
        LLVMBuildCall2(B, type_convert(F_ty), F, args, arg_count,
                       F_ty->return_ty->kind == TY_VOID ? "" : "funcall");
    free(args);
    return r;
  }
  case ND_LABEL_VAL: {
    LLVMBasicBlockRef bb = hashmap_get(&func_labels, node->unique_label);
    if (!bb) {
      // if bb exists, goto statement create this before.
      // we just reuse this, or we  create a new one
      bb = LLVMAppendBasicBlockInContext(C, F, node->unique_label);
      hashmap_put(&func_labels, node->unique_label, bb);
    }
    return LLVMBlockAddress(F, bb);
  }
  case ND_EXCH: {
    LLVMValueRef ptr = gen_expr(node->lhs);
    LLVMValueRef new_val = gen_expr(node->rhs);
    // return old val(pointee type)
    return LLVMBuildAtomicRMW(B, LLVMAtomicRMWBinOpXchg, ptr, new_val,
                              LLVMAtomicOrderingSequentiallyConsistent, false);
  }
  case ND_CAS: {
    LLVMValueRef ptr = gen_expr(node->cas_addr);
    LLVMValueRef old_ptr = gen_expr(node->cas_old);
    LLVMValueRef new_val = gen_expr(node->cas_new);
    LLVMValueRef old_val = LLVMBuildLoad2(
        B, type_convert(node->cas_old->ty->base), old_ptr, "cas_load_old_val");
    LLVMValueRef result = LLVMBuildAtomicCmpXchg(
        B, ptr, old_val, new_val, LLVMAtomicOrderingSequentiallyConsistent,
        LLVMAtomicOrderingSequentiallyConsistent, false);
    LLVMValueRef actual_old_val =
        LLVMBuildExtractValue(B, result, 0, "cas_actual_old");
    LLVMBuildStore(B, actual_old_val, old_ptr);

    LLVMValueRef success = LLVMBuildExtractValue(B, result, 1, "cas_success");
    return LLVMBuildZExt(B, success, LLVMInt8TypeInContext(C),
                         "cas_success_ext");
  }
  case ND_ADD:
    if (is_flonum(node->lhs->ty)) {
      return LLVMBuildFAdd(B, gen_expr(node->lhs), gen_expr(node->rhs), "fadd");
    } else {
      return LLVMBuildAdd(B, gen_expr(node->lhs), gen_expr(node->rhs), "add");
    }
  case ND_SUB: {
    if (is_flonum(node->lhs->ty)) {
      return LLVMBuildFSub(B, gen_expr(node->lhs), gen_expr(node->rhs), "fsub");
    } else {
      return LLVMBuildSub(B, gen_expr(node->lhs), gen_expr(node->rhs), "sub");
    }
  }
  case ND_MUL: {
    if (is_flonum(node->lhs->ty)) {
      return LLVMBuildFMul(B, gen_expr(node->lhs), gen_expr(node->rhs), "fmul");
    } else {
      return LLVMBuildMul(B, gen_expr(node->lhs), gen_expr(node->rhs), "mul");
    }
  }
  case ND_DIV: {
    if (is_flonum(node->lhs->ty)) {
      return LLVMBuildFDiv(B, gen_expr(node->lhs), gen_expr(node->rhs), "fdiv");
    } else {
      if (node->lhs->ty->is_unsigned)
        return LLVMBuildUDiv(B, gen_expr(node->lhs), gen_expr(node->rhs),
                             "udiv");
      else
        return LLVMBuildSDiv(B, gen_expr(node->lhs), gen_expr(node->rhs),
                             "sdiv");
    }
  }
  case ND_MOD: {
    assert(is_integer(node->lhs->ty));
    if (node->lhs->ty->is_unsigned)
      return LLVMBuildURem(B, gen_expr(node->lhs), gen_expr(node->rhs), "umod");
    else
      return LLVMBuildSRem(B, gen_expr(node->lhs), gen_expr(node->rhs), "smod");
  }
  case ND_BITAND: {
    assert(is_integer(node->lhs->ty));
    return LLVMBuildAnd(B, gen_expr(node->lhs), gen_expr(node->rhs), "bitand");
  }
  case ND_BITOR: {
    assert(is_integer(node->lhs->ty));
    return LLVMBuildOr(B, gen_expr(node->lhs), gen_expr(node->rhs), "bitor");
  }
  case ND_BITXOR: {
    assert(is_integer(node->lhs->ty));
    return LLVMBuildXor(B, gen_expr(node->lhs), gen_expr(node->rhs), "bitxor");
  }
  case ND_EQ:
  case ND_NE:
  case ND_LT:
  case ND_LE: {
    int fop = 0;
    int iop = 0;
    switch (node->kind) {
    case ND_EQ:
      fop = LLVMRealOEQ;
      iop = LLVMIntEQ;
      break;
    case ND_NE:
      fop = LLVMRealONE;
      iop = LLVMIntNE;
      break;
    case ND_LT:
      fop = LLVMRealOLT;
      if (node->lhs->ty->is_unsigned)
        iop = LLVMIntULT;
      else
        iop = LLVMIntSLT;
      break;
    case ND_LE:
      fop = LLVMRealOLE;
      if (node->lhs->ty->is_unsigned)
        iop = LLVMIntULE;
      else
        iop = LLVMIntSLE;
      break;
    }

    if (is_flonum(node->lhs->ty)) {
      return LLVMBuildFCmp(B, fop, gen_expr(node->lhs), gen_expr(node->rhs),
                           "fcmp");
    } else {
      return LLVMBuildICmp(B, iop, gen_expr(node->lhs), gen_expr(node->rhs),
                           "cmp");
    }
  }
  case ND_SHL: {
    // LLVM requires both sides to have the same type, so we need cast rhs
    LLVMValueRef rhs = gen_expr(node->rhs);
    rhs = cast(rhs, node->rhs->ty, node->lhs->ty, node->tok);
    return LLVMBuildShl(B, gen_expr(node->lhs), rhs, "shl");
  }
  case ND_SHR: {
    // LLVM requires both sides to have the same type, so we need cast rhs
    LLVMValueRef rhs = gen_expr(node->rhs);
    rhs = cast(rhs, node->rhs->ty, node->lhs->ty, node->tok);
    if (node->lhs->ty->is_unsigned)
      return LLVMBuildLShr(B, gen_expr(node->lhs), rhs, "lshr");
    else
      return LLVMBuildAShr(B, gen_expr(node->lhs), rhs, "ashr");
  }
  }
  error_tok(node->tok, "invalid expression");
}

static LLVMValueRef gen_stmt(Node *node) {
  switch (node->kind) {
  case ND_BLOCK: {
    new_block("block");
    for (Node *n = node->body; n; n = n->next)
      gen_stmt(n);
    return NULL;
  }
  case ND_RETURN: {
    new_block("return");
    LLVMBuildRet(B, gen_expr(node->lhs));
    return NULL;
  }
  case ND_EXPR_STMT: {
    new_block("expr_stmt");
    return gen_expr(node->lhs);
  }
  case ND_IF: {
    new_block("if");
    LLVMValueRef cond = gen_expr(node->cond);
    cond = cmp_nz(cond);
    LLVMBasicBlockRef bb_then = LLVMAppendBasicBlockInContext(C, F, "if_then");
    LLVMBasicBlockRef bb_else = LLVMAppendBasicBlockInContext(C, F, "if_else");
    LLVMBasicBlockRef bb_merge =
        LLVMAppendBasicBlockInContext(C, F, "if_merge");
    LLVMBuildCondBr(B, cond, bb_then, bb_else);
    LLVMPositionBuilderAtEnd(B, bb_then);
    gen_stmt(node->then);
    LLVMBuildBr(B, bb_merge);
    LLVMPositionBuilderAtEnd(B, bb_else);
    if (node->_else) {
      gen_stmt(node->_else);
    }
    LLVMBuildBr(B, bb_merge);
    LLVMPositionBuilderAtEnd(B, bb_merge);
    return NULL;
  }
  case ND_FOR: {
    new_block("for");

    LLVMBasicBlockRef bb_merge =
        LLVMAppendBasicBlockInContext(C, F, "for_merge");
    LLVMBasicBlockRef bb_cond = LLVMAppendBasicBlockInContext(C, F, "for_cond");
    LLVMBasicBlockRef bb_body = LLVMAppendBasicBlockInContext(C, F, "for_body");
    LLVMBasicBlockRef bb_inc = LLVMAppendBasicBlockInContext(C, F, "for_inc");
    hashmap_put(&func_labels, node->break_label, bb_merge);
    hashmap_put(&func_labels, node->cont_label, bb_inc);
    if (node->init) {
      gen_stmt(node->init);
    }
    LLVMBuildBr(B, bb_cond);
    LLVMPositionBuilderAtEnd(B, bb_cond);
    if (node->cond) {
      LLVMValueRef cond = gen_expr(node->cond);
      cond = cmp_nz(cond);
      LLVMBuildCondBr(B, cond, bb_body, bb_merge);
    } else {
      LLVMBuildBr(B, bb_body);
    }
    LLVMPositionBuilderAtEnd(B, bb_body);
    gen_stmt(node->then);
    LLVMBuildBr(B, bb_inc);
    LLVMPositionBuilderAtEnd(B, bb_inc);

    if (node->inc)
      gen_expr(node->inc);
    LLVMBuildBr(B, bb_cond);

    LLVMPositionBuilderAtEnd(B, bb_merge);
    return NULL;
  }
  case ND_DO: {
    new_block("do");
    LLVMBasicBlockRef bb_body = LLVMAppendBasicBlockInContext(C, F, "do_body");
    LLVMBasicBlockRef bb_cond = LLVMAppendBasicBlockInContext(C, F, "do_cond");
    LLVMBasicBlockRef bb_merge =
        LLVMAppendBasicBlockInContext(C, F, "do_merge");
    hashmap_put(&func_labels, node->break_label, bb_merge);
    hashmap_put(&func_labels, node->cont_label, bb_cond);
    LLVMBuildBr(B, bb_body);
    LLVMPositionBuilderAtEnd(B, bb_body);
    gen_stmt(node->then);
    LLVMBuildBr(B, bb_cond);
    LLVMPositionBuilderAtEnd(B, bb_cond);
    LLVMValueRef cond = gen_expr(node->cond);
    cond = cmp_nz(cond);
    LLVMBuildCondBr(B, cond, bb_body, bb_merge);
    LLVMPositionBuilderAtEnd(B, bb_merge);
    return NULL;
  }
  case ND_LABEL: {
    new_block("labeled_stmt");
    LLVMBasicBlockRef bb = hashmap_get(&func_labels, node->unique_label);
    if (!bb) {
      // if bb exists, goto statement create this before.
      // we just reuse this, or we  create a new one
      bb = LLVMAppendBasicBlockInContext(C, F, node->unique_label);
      hashmap_put(&func_labels, node->unique_label, bb);
    }
    LLVMBuildBr(B, bb);
    LLVMPositionBuilderAtEnd(B, bb);
    return gen_stmt(node->lhs);
  }
  case ND_GOTO: {
    new_block("goto");

    LLVMBasicBlockRef bb = hashmap_get(&func_labels, node->unique_label);
    if (!bb) {
      // if bb exists, labeled statement create this before.
      // we just reuse this, or we  create a new one
      bb = LLVMAppendBasicBlockInContext(C, F, node->unique_label);
      hashmap_put(&func_labels, node->unique_label, bb);
    }
    LLVMBuildBr(B, bb);
    // we keep current Builder position because the later code is
    // not necessary just like those ir after br in same block.
    return NULL;
  }
  case ND_SWITCH:
  case ND_CASE:
  case ND_GOTO_EXPR:
  case ND_ASM:
  default:
    unreachable();
  }
}

static void codegen_build_function(Obj *var) {
  F = LLVMGetNamedFunction(M, var->name);
  assert(F);
  // prologue
  LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(C, F, "entry");
  LLVMPositionBuilderAtEnd(B, entry);
  size_t args_count = 0;
  for (Obj *p = var->params; p; p = p->next) {
    LLVMValueRef local_arg = LLVMBuildAlloca(B, type_convert(p->ty), p->name);
    LLVMValueRef arg = LLVMGetParam(F, args_count++);

    // store arg to alloca variable
    LLVMBuildStore(B, arg, local_arg);
    p->codegen_data = (intptr_t)local_arg;
  }
  for (Obj *v = var->locals; v; v = v->next) {
    if (v->codegen_data)
      continue;
    LLVMValueRef lv = LLVMBuildAlloca(B, type_convert(v->ty), v->name);
    v->codegen_data = (intptr_t)lv;
  }
  gen_stmt(var->body);
  if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(B))) {
    // If there isn't any terminator(return) in the last BB, create a new one
    if (var->ty->return_ty->kind == TY_VOID) {
      LLVMBuildRetVoid(B);
    } else {
      LLVMBuildRet(B, LLVMConstNull(type_convert(var->ty->return_ty)));
    }
  }
  F = NULL;
  hashmap_clear(&func_labels);
}

// stage 2. initialize global variable
static void codegen_global_init(Obj *prog) {
  for (Obj *var = prog; var; var = var->next) {
    if (!var->is_function) {
      LLVMValueRef v = (LLVMValueRef)var->codegen_data;
      assert(v);
      if (var->init) {
        LLVMSetInitializer(v, init_global_data(var->ty, var->init));
      } else {
        LLVMSetInitializer(v, LLVMConstNull(type_convert(var->ty)));
      }
      continue;
    }
    if (var->is_function && var->body) {
      codegen_build_function(var);
      continue;
    }
  }
}

void declare_built_function() {
  LLVMTypeRef ptr = LLVMPointerTypeInContext(C, 0);
  LLVMTypeRef i8 = LLVMInt8TypeInContext(C);
  LLVMTypeRef i64 = LLVMInt64TypeInContext(C);
  LLVMTypeRef immarg = LLVMInt1TypeInContext(C);
  LLVMTypeRef param_tys[] = {ptr, i8, i64, immarg};
  llvm_memset_declare_ty =
      LLVMFunctionType(LLVMVoidTypeInContext(C), param_tys, 4, false);

  llvm_memset_declare =
      LLVMAddFunction(M, "llvm.memset.p0.i64", llvm_memset_declare_ty);
}

void codegen(Obj *prog, FILE *out) {
  C = LLVMContextCreate();
  M = LLVMModuleCreateWithNameInContext(get_current_file()->name, C);
  B = LLVMCreateBuilderInContext(C);

  declare_built_function();

  codegen_global_declare(prog);

  codegen_global_init(prog);
  // print to out
  char *ir = LLVMPrintModuleToString(M);
  fputs(ir, out);
  fflush(out);
  LLVMDisposeMessage(ir);

  // cleanup
  LLVMDisposeBuilder(B);
  LLVMDisposeModule(M);
  LLVMContextDispose(C);
}

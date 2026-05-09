
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
static LLVMValueRef llvm_memset_declare;
static LLVMValueRef gen_expr(Node *node);
static LLVMValueRef gen_stmt(Node *node);

static LLVMTypeRef type_convert(Type *ty) {
  switch (ty->kind) {
  case TY_VOID:
    return LLVMVoidTypeInContext(C);
  case TY_BOOL:
    return LLVMInt1TypeInContext(C);
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
    // TODO:union
    unreachable();
  } else if (ty->kind == TY_STRUCT) {
    size_t member_count = next_iter_count(ty->members);
    LLVMValueRef *cv_array = calloc(ty->array_len, sizeof(LLVMValueRef));
    {
      size_t cv_array_idx = 0;
      for (Member *m = ty->members; m; m = m->next) {
        // TODO: bitfield
        assert(!m->is_bitfield);
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
    char **label = NULL;
    int64_t eval_val = eval2(init->expr, &label);
    if (label) {
      // Relocation pointer, real data: label + eval_val
      LLVMValueRef target_val = LLVMGetNamedGlobal(M, *label);
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

enum { I8, I16, I32, I64, U8, U16, U32, U64, F32, F64, F128 };

static int getTypeId(Type *ty) {
  switch (ty->kind) {
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
  }
  return U64;
}
enum { CAST_NOP = -1 };

static int cast_table[11][11] = {
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
        },
    [I16] =
        {
            [I8] = LLVMTrunc,
            [I16] = CAST_NOP,
            [I32] = LLVMSExt,
            [I64] = LLVMSExt,
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
        },
};

static LLVMValueRef cast(LLVMValueRef v, Type *from, Type *to) {
  if (to->kind == TY_VOID) {
    unreachable();
  }

  int from_id = getTypeId(from);
  int to_id = getTypeId(to);
  int op = cast_table[from_id][to_id];
  if (op == CAST_NOP) {
    return v;
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
      // TODO: understand
      return gen_expr(node);
    }
    break;
  case ND_ASSIGN:
  case ND_COND:
    if (node->ty->kind == TY_STRUCT || node->ty->kind == TY_UNION)
      return gen_expr(node);
    break;
  case ND_VLA_PTR:
    // TODO:vla ptr
    unreachable();
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
    // TODO: struct
    unreachable();
  default:
    break;
  }
  LLVMBuildStore(B, v, ptr);
}

static void llvm_memset2(LLVMValueRef ptr, LLVMValueRef byte, LLVMValueRef n,
                         LLVMValueRef immarg) {
  LLVMBuildCall2(B, LLVMTypeOf(llvm_memset_declare), llvm_memset_declare,
                 (LLVMValueRef[4]){ptr, byte, n, immarg}, 4,
                 "llvm_memset_call");
}

static void llvm_memset(LLVMValueRef ptr, char byte, size_t n,
                        bool is_volatile) {
  llvm_memset2(ptr, LLVMConstInt(LLVMInt8TypeInContext(C), byte, false),
               LLVMConstInt(LLVMInt64TypeInContext(C), n, false),
               LLVMConstInt(LLVMInt1TypeInContext(C), is_volatile, false));
}

static LLVMValueRef cmp_zero(LLVMValueRef v) {
  LLVMTypeRef vty = LLVMTypeOf(v);
  LLVMTypeKind vk = LLVMGetTypeKind(vty);
  LLVMValueRef zero = LLVMConstNull(LLVMTypeOf(v));
  if (vk == LLVMIntegerTypeKind || vk == LLVMPointerTypeKind) {
    return LLVMBuildICmp(B, LLVMIntNE, v, zero, "cmp_zero_i");
  }
  return LLVMBuildFCmp(B, LLVMRealONE, v, zero, "cmp_zero_f");
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
    return LLVMBuildNeg(B, gen_expr(node->lhs), "neg");
  }
  case ND_CAST: {
    return cast(gen_expr(node->lhs), node->lhs->ty, node->ty);
  }
  case ND_VAR: {
    return load(node->ty, gen_addr(node));
  }
  case ND_DEREF: {
    return load(node->ty, gen_addr(node->lhs));
  }
  case ND_ADDR: {
    return gen_addr(node->lhs);
  }
  case ND_MEMBER: {
    return load(node->ty, gen_addr(node));
  }
  case ND_STMT_EXPR: {
    new_block("stmt_expr");
    LLVMValueRef r;
    // statement expression will return the last expression statement
    for (Node *n = node->body; n; n = n->next)
      r = gen_stmt(n);
    if (!r) {
      error_tok(node->tok, "This statement expression returns the void type");
    }
    return r;
  }
  case ND_COMMA:
    gen_expr(node->lhs);
    return gen_expr(node->rhs);
  case ND_ASSIGN: {
    LLVMValueRef ptr = gen_addr(node->lhs);
    LLVMValueRef v = gen_expr(node->rhs);
    // TODO: bitfield
    store(node->ty, ptr, v);
    // load again
    return load(node->lhs->ty, ptr);
  }
  case ND_MEMZERO: {
    // TODO: test
    assert(node->var->codegen_data);
    llvm_memset((LLVMValueRef)node->var->codegen_data, 0, node->var->ty->size,
                false);
    return NULL;
  }
  case ND_COND: {
    LLVMValueRef cond = gen_expr(node->cond);
    cond = cmp_zero(cond);
    LLVMBasicBlockRef bb_then =
        LLVMAppendBasicBlockInContext(C, F, "cond_then");
    LLVMBasicBlockRef bb_else =
        LLVMAppendBasicBlockInContext(C, F, "cond_else");
    LLVMBasicBlockRef bb_merge =
        LLVMAppendBasicBlockInContext(C, F, "cond_merge");
    LLVMBuildCondBr(B, cond, bb_then, bb_else);
    LLVMPositionBuilderAtEnd(B, bb_then);
    LLVMValueRef then_v = gen_expr(node->then);
    LLVMBuildBr(B, bb_merge);
    LLVMPositionBuilderAtEnd(B, bb_else);
    LLVMValueRef else_v = gen_expr(node->_else);
    LLVMBuildBr(B, bb_merge);
    LLVMPositionBuilderAtEnd(B, bb_merge);
    LLVMValueRef phi =
        LLVMBuildPhi(B, type_convert(node->ty), "cond_merge_phi");
    LLVMAddIncoming(phi, (LLVMValueRef[]){then_v, else_v},
                    (LLVMBasicBlockRef[]){bb_then, bb_else}, 2);
    return phi;
  }
  default: {
    unreachable();
  }
  }
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
    return gen_expr(node->lhs);
  }
  default:
    unreachable();
  }
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
      F = LLVMGetNamedFunction(M, var->name);
      assert(F);
      // prologue
      LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(C, F, "entry");
      LLVMPositionBuilderAtEnd(B, entry);
      size_t args_count = 0;
      for (Obj *lv = var->params; lv; lv = lv->next) {
        LLVMValueRef local_arg =
            LLVMBuildAlloca(B, type_convert(lv->ty), lv->name);
        LLVMValueRef arg = LLVMGetParam(F, args_count++);

        // store arg to alloca variable
        LLVMBuildStore(B, arg, local_arg);
        lv->codegen_data = (intptr_t)local_arg;
      }
      gen_stmt(var->body);
      F = NULL;
      continue;
    }
  }
}

void declare_built_function() {
  unsigned memset_id = LLVMLookupIntrinsicID("llvm.memset.p0.i64", 20);
  LLVMTypeRef ptr = LLVMPointerTypeInContext(C, 0);
  LLVMTypeRef i8 = LLVMInt8TypeInContext(C);
  LLVMTypeRef i64 = LLVMInt64TypeInContext(C);
  LLVMTypeRef immarg = LLVMInt1TypeInContext(C);
  LLVMTypeRef ParamTys[] = {ptr, i8, i64, immarg};
  llvm_memset_declare = LLVMGetIntrinsicDeclaration(M, memset_id, ParamTys, 1);
  assert(llvm_memset_declare);
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

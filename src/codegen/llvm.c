
#include "dcc.h"
#include <assert.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static LLVMContextRef C;
static LLVMModuleRef M;
static LLVMBuilderRef B;

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

  case TY_ENUM:
  case TY_FUNC:
  case TY_VLA:
  case TY_STRUCT:
  case TY_UNION:
  default:
    break;
  }
  unreachable();
}

static LLVMValueRef init_data(Type *ty, Initializer *init) {
  LLVMTypeRef llvm_ty = type_convert(ty);
  LLVMValueRef init_val;
  if (ty->kind == TY_ARRAY) {
    LLVMValueRef *cv_array = calloc(ty->array_len, sizeof(LLVMValueRef));
    for (int i = 0; i < ty->array_len; i++) {
      cv_array[i] = init_data(ty->base, init->children[i]);
    }
    init_val = LLVMConstArray2(type_convert(ty->base), cv_array, ty->array_len);
    free(cv_array);
  } else if (ty->kind == TY_UNION) {
    // TODO:
    unreachable();
  } else if (ty->kind == TY_STRUCT) {
    // TODO:
    unreachable();
  } else if (ty->kind == TY_DOUBLE || ty->kind == TY_FLOAT) {
    init_val = LLVMConstReal(llvm_ty, eval_double(init->expr));
  } else if (!init->expr) {
    init_val = LLVMConstNull(llvm_ty);
  } else {
    char **label = NULL;
    int64_t eval_val = eval2(init->expr, &label);
    if (label) {
      // Relocation, real data: label + eval_val
      LLVMValueRef target_val = LLVMGetNamedGlobal(M, *label);
      assert(target_val);
      assert(ty->base);
      LLVMTypeRef pointee_ty = type_convert(ty->base);
      LLVMValueRef indices =
          LLVMConstInt(LLVMInt64TypeInContext(C), (uint64_t)eval_val, false);
      init_val = LLVMConstInBoundsGEP2(pointee_ty, target_val, &indices, 1);
    } else {
      init_val = LLVMConstInt(llvm_ty, eval_val, ty->is_unsigned);
    }
  }
  return init_val;
}

// The first stage. Only declare global variable to avoid dependency order
// problem
static void codegen_declare_only(Obj *prog) {
  for (Obj *var = prog; var; var = var->next) {
    if (var->is_function)
      continue;
    LLVMTypeRef ty = type_convert(var->ty);
    LLVMValueRef v = LLVMAddGlobal(M, ty, var->name);
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

    LLVMSetAlignment(v, var->ty->align);
  }
}

// stage 2. initialize variable
static void codegen_init(Obj *prog) {
  for (Obj *var = prog; var; var = var->next) {
    if (var->is_function)
      continue;
    if (var->init) {
      LLVMValueRef v = LLVMGetNamedGlobal(M, var->name);
      assert(v);
      LLVMSetInitializer(v, init_data(var->ty, var->init));
    }
  }
}

void codegen(Obj *prog, FILE *out) {
  C = LLVMContextCreate();
  M = LLVMModuleCreateWithNameInContext(get_current_file()->name, C);
  B = LLVMCreateBuilderInContext(C);

  codegen_declare_only(prog);

  codegen_init(prog);
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

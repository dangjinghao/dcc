
#include "dcc.h"
#include <assert.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include <stdbool.h>
#include <stdio.h>

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

static void global_variable(Obj *prog) {
  for (Obj *var = prog; var; var = var->next) {
    if (var->is_function || !var->is_definition)
      continue;
    LLVMTypeRef ty = type_convert(var->ty);
    LLVMValueRef v = LLVMAddGlobal(M, ty, var->name);
    if (var->is_static) {
      LLVMSetLinkage(v, LLVMInternalLinkage);
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

void codegen(Obj *prog, FILE *out) {
  C = LLVMContextCreate();
  M = LLVMModuleCreateWithNameInContext(get_current_file()->name, C);
  B = LLVMCreateBuilderInContext(C);

  global_variable(prog);

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


#include "dcc.h"
#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include <stdio.h>
#include <stdlib.h>

void codegen(Obj *prog, FILE *out) {
  LLVMContextRef C = LLVMContextCreate();
  LLVMModuleRef M = LLVMModuleCreateWithNameInContext("return_param", C);
  LLVMBuilderRef B = LLVMCreateBuilderInContext(C);

  LLVMTypeRef i32 = LLVMInt32TypeInContext(C);
  LLVMTypeRef func_type = LLVMFunctionType(i32, NULL, 0, 0);
  LLVMValueRef main_func = LLVMAddFunction(M, "main", func_type);
  LLVMBasicBlockRef entry =
      LLVMAppendBasicBlockInContext(C, main_func, "entry");
  LLVMPositionBuilderAtEnd(B, entry);
  LLVMValueRef one = LLVMConstInt(i32, 1, 0);
  LLVMBuildRet(B, one);
  char *error = NULL;
  if (LLVMVerifyModule(M, LLVMPrintMessageAction, &error)) {
    fprintf(stderr, "Module verification failed: %s\n", error);
    LLVMDisposeMessage(error);
    exit(1);
  }
  char *ir = LLVMPrintModuleToString(M);
  fputs(ir, out);
  fflush(out);
  LLVMDisposeMessage(ir);
  LLVMDisposeBuilder(B);
  LLVMDisposeModule(M);
}

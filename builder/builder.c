#include "builder.h"
#include "ast.h"
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>

const char *GLOBAL_STATIC_FMT = "__%s.%ld";
const char *FUNCTION_STATIC_FMT = "__%s.%s.%ld";

builder builder_create(builder b, char *module_name, astn n) {
  b->context = LLVMContextCreate();
  b->module = LLVMModuleCreateWithNameInContext(module_name, b->context);
  b->builder = LLVMCreateBuilderInContext(b->context);
  return b;
}

void builder_destroy(builder b) {
  LLVMDisposeBuilder(b->builder);
  LLVMDisposeModule(b->module);
  LLVMContextDispose(b->context);
}
#include "builder.h"
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>

builder builder_create(builder b, char *module_name, astn n) {
  b->context = LLVMContextCreate();
  b->module = LLVMModuleCreateWithNameInContext(module_name, b->context);
  b->builder = LLVMCreateBuilderInContext(b->context);
  b->ast = n;
  return b;
}

void builder_destroy(builder b) {
  LLVMDisposeBuilder(b->builder);
  LLVMDisposeModule(b->module);
  LLVMContextDispose(b->context);
}
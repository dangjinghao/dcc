#include "builder.h"
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>

const char *STATIC_VAR_FMT = "%s.%ld";
const char *STRUCT_FMT = "struct.%s.%ld";
const char *VAR_FMT = "v.%ld";
builder builder_new(builder b, char *module_name) {
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

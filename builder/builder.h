#ifndef BUILDER_H
#define BUILDER_H
#include "ast.h"
#include <llvm-c/Types.h>
typedef struct builder {
  LLVMBuilderRef builder;
  LLVMModuleRef module;
  LLVMContextRef context;
} *builder;
void builder_destroy(builder b);
builder builder_create(builder b, char *module_name);

LLVMValueRef build_declaration(builder b, astn n);
LLVMTypeRef build_convert_base_type(astn n, builder b);
LLVMTypeRef build_variable_declaration_type(builder b, astn n);

extern const char *STATIC_VAR_FMT;
extern const char *STRUCT_FMT;
#endif
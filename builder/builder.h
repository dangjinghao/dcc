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

void build_declaration(builder b, astn n);
LLVMTypeRef build_convert_base_type(astn n, builder b);
LLVMTypeRef build_variable_declaration_type(builder b, astn n);
llvm_typed_value build_expression(builder b, astn n);
LLVMTypeRef build_get_declaration_points_to_type(builder b, llvm_typed_value v);
llvm_typed_value *build_2_values_type_upper_cast(builder b,
                                                 llvm_typed_value *values);
extern const char *STATIC_VAR_FMT;
extern const char *STRUCT_FMT;
extern const char *VAR_FMT;
#endif
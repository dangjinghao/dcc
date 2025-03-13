#ifndef BUILDER_H
#define BUILDER_H
#include "ast.h"
#include <llvm-c/Types.h>
typedef struct builder {
  LLVMBuilderRef builder;
  LLVMModuleRef module;
  LLVMContextRef context;
  slist symtab;
} *builder;
void builder_destroy(builder b);
builder builder_new(builder b, char *module_name, slist symtab);

slist build_base_type_chain_by_lit(enum tok_type type);
slist build_type_chain_expr_primary(astn n);
typed_value build_type_convert_to(builder b, typed_value v, slist type_chain);
int build_type_compare_promote_level(astn lhs_base_type, astn rhs_base_type);

void build_trans_unit(builder b, slist symtab);
typed_value build_lvalue_exprssion(builder b, astn n);
typed_value build_declaration(builder b, astn n);
LLVMTypeRef build_convert_base_type(builder b, astn n);
LLVMTypeRef build_variable_declaration_type(builder b, astn n);
typed_value build_expression(builder b, astn n);
slist build_type_chain_copy(slist type_chain);
slist build_type_get_points_to_type_chian(builder b, slist type_chain);
typed_value *build_type_2_values_type_upper_cast(builder b,
                                                 typed_value *values);
extern const char *STATIC_VAR_FMT;
extern const char *STRUCT_FMT;
extern const char *VAR_FMT;
#endif
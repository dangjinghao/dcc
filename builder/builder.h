#ifndef BUILDER_H
#define BUILDER_H
#include "ast.h"
#include "dynarray/dynarray.h"
#include <llvm-c/Types.h>
typedef struct builder {
  LLVMBuilderRef builder;
  LLVMModuleRef module;
  LLVMContextRef context;
  slist symtab;
  // current function
  LLVMValueRef fn;
  // goto label
  struct slist labels;
} *builder;

typedef struct label {
  bool defined;
  sds name;
  LLVMBasicBlockRef block;
} *label;

void builder_destroy(builder b);
builder builder_new(builder b, char *module_name, slist symtab);
label builder_label_find(builder b, sds name);
void builder_label_list_free(builder b);
void builder_check_label_list_undefined(builder b);
label builder_label_new(builder b, sds name);
slist build_base_type_chain_by_lit(enum tok_type type);
slist build_type_chain_expr_primary(astn n);
typed_value build_type_convert_to(builder b, typed_value v, slist type_chain);
int build_type_compare_promote_level(astn lhs_base_type, astn rhs_base_type);

dynarray build_function_parameters_type(builder b, astn params, dynarray arr);
void build_trans_unit(builder b, slist symtab);
typed_value build_lvalue_exprssion(builder b, astn n);
typed_value build_declaration(builder b, astn n);
LLVMTypeRef build_function_llvm_type_by_ast(builder b, astn n);
LLVMTypeRef build_convert_base_type(builder b, astn n);
LLVMTypeRef build_variable_declaration_type(builder b, astn n);
typed_value build_expression(builder b, astn n);
slist build_type_chain_copy(slist type_chain);
slist build_type_get_points_to_type_chian(builder b, slist type_chain);
slist build_function_return_type_chain(builder b, astn n);
typed_value *build_type_2_values_type_upper_cast(builder b,
                                                 typed_value *values);
void build_statement(builder b, astn n);
void build_block(builder b, astn blk);
extern const char *STATIC_VAR_FMT;
extern const char *STRUCT_FMT;
extern const char *VAR_FMT;
extern const char *GOTO_BLK_FMT;
#endif
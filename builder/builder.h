#ifndef BUILDER_H
#define BUILDER_H
#include "ast.h"
#include "dynarray/dynarray.h"
#include "typed_value/typed_value.h"
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
typedef struct builder {
  LLVMBuilderRef builder;
  LLVMModuleRef module;
  LLVMContextRef context;
  slist symtab;
  // current function, used for append basic block only
  LLVMValueRef fn;
  // goto label
  struct slist labels;
} *builder;

typedef struct label {
  bool defined;
  sds name;
  LLVMBasicBlockRef block;
} *goto_label;

typedef LLVMValueRef (*llvm_func_t)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef,
                                    const char *);

void builder_destroy(builder b);
builder builder_new(builder b, char *module_name, slist symtab);
goto_label builder_label_find(builder b, sds name);
void builder_label_list_free(builder b);
void builder_check_label_list_undefined(builder b);
goto_label builder_label_new(builder b, sds name);
slist build_base_type_chain_by_lit(enum tok_type type);
slist build_type_chain_expr_primary(astn n);
typed_value build_type_convert_to(builder b, typed_value v, slist type_chain);
int build_type_compare_promote_level(astn lhs_base_type, astn rhs_base_type);

dynarray build_function_parameters_type(builder b, astn params, dynarray arr);
void build_trans_unit(builder b, slist symtab);

LLVMValueRef build_value_eq0(builder b, typed_value v);
LLVMValueRef build_value_ne0(builder b, typed_value v);
void build_value_store(builder b, typed_value v, typed_value ptr);
typed_value build_value_load(builder b, typed_value v);
typed_value build_value_expr_binop_template(builder b, typed_value lhs,
                                            typed_value rhs,
                                            llvm_func_t llvm_build_f[2],
                                            char *f_names[2]);
typed_value build_value_expr_binop_su_template(builder b, typed_value lhs,
                                               typed_value rhs,
                                               llvm_func_t llvm_build_f[2],
                                               char *f_names[2]);
typed_value build_value_expr_binop_bit_template(builder b, typed_value lhs,
                                                typed_value rhs,
                                                llvm_func_t llvm_build_f,
                                                char *f_name);

typed_value build_value_expr_binop_div(builder b, typed_value lhs,
                                       typed_value rhs);

bool build_expr_is_binop_with_ptr(typed_value lhs, typed_value rhs);
typed_value build_lvalue_expression(builder b, astn n);
typed_value build_expr_binop(builder b, astn n);
typed_value build_expression(builder b, astn n);

typed_value build_declaration(builder b, astn n);
LLVMTypeRef build_function_llvm_type_by_ast(builder b, astn n);
LLVMTypeRef build_convert_base_type(builder b, astn n);
LLVMTypeRef build_variable_declaration_type(builder b, astn n);
slist build_type_chain_copy(slist type_chain);
slist build_type_get_points_to_type_chian(builder b, slist type_chain);
slist build_function_return_type_chain(builder b, astn n);
slist build_type_chain_add_pointer(builder b, slist type_chain);
int build_type_get_struct_member(astn struct_declaration, sds name,
                                 astn *result);
LLVMTypeRef build_convert_struct_type(builder b, astn n);
typed_value *build_type_2_values_type_upper_cast(builder b,
                                                 typed_value *values);

void build_statement(builder b, astn n);
void build_block(builder b, astn blk);

LLVMValueRef build_value_cmp0(builder b, typed_value v, LLVMIntPredicate iPred,
                              LLVMRealPredicate fPred, const char *label);
LLVMValueRef build_value_ne0(builder b, typed_value v);

extern const char *STATIC_VAR_FMT;
extern const char *STRUCT_FMT;
extern const char *VAR_FMT;
extern const char *GOTO_BLK_FMT;
extern const char *CASE_BLK_FMT;
#endif
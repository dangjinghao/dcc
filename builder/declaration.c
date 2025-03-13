#include "ast.h"
#include "builder.h"
#include "dynarray/dynarray.h"
#include "grammar.h"
#include "lexer.h"
#include "log/log.h"
#include "macro/macro.h"
#include "sds/sds.h"
#include "slist/slist.h"
#include <assert.h>
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include <stdbool.h>
#include <stddef.h>

dynarray build_struct_member_declaration_type(astn n, builder b, dynarray arr) {
  assert(n->type == ast_struct_union_declaration);
  astn struct_declaration;
  slist_foreach(&n->struct_union_declaration.member_declarations,
                struct_declaration) {
    LLVMTypeRef t = build_variable_declaration_type(b, struct_declaration);
    dynarray_add(arr, &t);
  }
  return arr;
}

LLVMTypeRef build_convert_struct_type(builder b, astn n) {
  LLVMTypeRef t;
  if (n->type == ast_ref) {
    // the reference of existing struct definition
    n = n->ref;
    assert(n->type == ast_struct_union_declaration);
    assert(n->struct_union_declaration.V);
    t = n->struct_union_declaration.V;
    log_debug("refering the existed struct type:%s", LLVMGetStructName(t));
  } else {
    assert(n->type == ast_struct_union_declaration);
    struct dynarray dyn_elements;
    dynarray_default(&dyn_elements, sizeof(LLVMTypeRef));
    build_struct_member_declaration_type(n, b, &dyn_elements);
    size_t elements_count = dyn_elements.used;
    LLVMTypeRef *elements = dyn_elements.data;
    if (n->struct_union_declaration.ident) {
      sds name;
      name = sdscatprintf(sdsempty(), STRUCT_FMT,
                          n->struct_union_declaration.ident,
                          n->struct_union_declaration.uid);
      log_debug("create struct definition with name: %s", name);
      t = LLVMStructCreateNamed(b->context, name);
      LLVMStructSetBody(t, elements, elements_count, false);
      sdsfree(name);
    } else {
      log_debug("create anonymous struct definition");
      t = LLVMStructTypeInContext(b->context, elements, elements_count, false);
    }
    dynarray_free(&dyn_elements);
    assert(n->struct_union_declaration.V == NULL);
    n->struct_union_declaration.V = t;
  }
  return t;
}

LLVMTypeRef build_convert_base_type(builder b, astn n) {
  assert(n->type == ast_ctype);
  int t = n->ctype.type;
  auto c = b->context;
  switch (t) {
  case TOK_KW_VOID:
    return LLVMVoidTypeInContext(c);
  case TOK_KW_CHAR:
    static_assert(sizeof(char) == 1);
    return LLVMInt8TypeInContext(c);
  case TOK_KW_SHORT:
    static_assert(sizeof(short) == 2);
    return LLVMInt16TypeInContext(c);
  case TOK_KW_FLOAT:
    return LLVMFloatTypeInContext(c);
  case TOK_KW_INT:
    static_assert(sizeof(int) == 4);
    return LLVMInt32TypeInContext(c);
  case TOK_KW_LONG:
    static_assert(sizeof(long) == 8);
    return LLVMInt64TypeInContext(c);
  case TOK_KW_DOUBLE:
    return LLVMDoubleTypeInContext(c);
  case '*':
    return LLVMPointerTypeInContext(c, 0);
  case TOK_KW_STRUCT:
    return build_convert_struct_type(b, n->ctype.user_defined_type);
  default:
    break;
  }

  BUILDING();
  return NULL;
}

/**
 * @brief build a variable declaration type,
 * if the declaration is a function declaration, return the return type
 * 
 * @param b 
 * @param n 
 * @return LLVMTypeRef 
 */
LLVMTypeRef build_variable_declaration_type(builder b, astn n) {
  assert(n->type == ast_declaration);
  astn _t = g_get_declaration_base_type(n);
  if (_t->type == ast_parameters) {
    _t = g_get_function_return_base_type(n);
  } else if (_t->type == ast_expr_unary) {
    BUILDING();
  }
  return build_convert_base_type(b, _t);
}

/**
 * @brief generate a symbol name by the storage class and the scope
 * 
 * @param n 
 * @return char* 
 */
sds build_symbol_name(astn n) {
  assert(n->type == ast_declaration);
  astn decl_specs = g_get_declaration_specifier(n);
  if (g_is_declaration_in_function_scope(n)) {
    if (decl_specs->ctype.storage == TOK_KW_STATIC) {
      // static function variable
      assert(n->declaration.scope_ref->type == ast_declaration);
      assert(n->declaration.scope_ref->declaration.ident);
      return sdscatprintf(sdsempty(), STATIC_VAR_FMT, n->declaration.ident,
                          n->declaration.uid);
    } else {
      // those variables in function scope would drop their name
      return sdscatprintf(sdsempty(), VAR_FMT, n->declaration.uid);
    }
  } else {
    // global scope
    if (decl_specs->ctype.storage == TOK_KW_STATIC) {
      // static global variable
      return sdscatprintf(sdsempty(), STATIC_VAR_FMT, n->declaration.ident,
                          n->declaration.uid);
    } else {
      // extern or unknown storage class has the name same as its identifier
      return sdsdup(n->declaration.ident);
    }
  }
  return NULL;
}

void build_global_variable_init(LLVMValueRef pv, astn n,
                                LLVMTypeRef value_type) {
  if (!n->declaration.extdata) {
    log_debug("no initializer for %s, use default",
              LLVMGetValueName2(pv, &(size_t){}));
    LLVMSetInitializer(pv, LLVMConstNull(value_type));
  } else {
    BUILDING();
  }
}

LLVMValueRef build_global_variable(builder b, astn n) {
  assert(n->type == ast_declaration);
  sds sym_name = build_symbol_name(n);
  LLVMTypeRef value_type = build_variable_declaration_type(b, n);
  LLVMValueRef pv = LLVMAddGlobal(b->module, value_type, sym_name);
  sdsfree(sym_name);
  astn decl_specs = g_get_declaration_specifier(n);
  if (decl_specs->ctype.storage != TOK_KW_EXTERN) {
    build_global_variable_init(pv, n, value_type);
  }
  return pv;
}

void build_alloca_variable_init(builder b, astn n, LLVMValueRef pv) {
  if (n->declaration.extdata) {
    astn init = n->declaration.extdata;
    assert(init->type == ast_initializer);
    log_trace("alloca variable %s has initializer",
              LLVMGetValueName2(pv, &(size_t){}));
    auto v = build_expression(b, init->initializer.init);
    log_trace("try to cast the initializer to the variable type");
    build_type_convert_to(b, v, &n->declaration.type_chain);
    LLVMBuildStore(b->builder, v->v, pv);
  }
}

LLVMValueRef build_alloca_variable(builder b, astn n) {
  assert(n->type == ast_declaration);
  sds sym_name = build_symbol_name(n);
  LLVMTypeRef value_type = build_variable_declaration_type(b, n);
  auto pv = LLVMBuildAlloca(b->builder, value_type, sym_name);
  sdsfree(sym_name);
  build_alloca_variable_init(b, n, pv);
  return pv;
}

dynarray build_function_parameters_type(builder b, astn params, dynarray arr) {
  assert(params->type == ast_parameters);
  astn param_declaration;
  slist_foreach(&params->parameters.list, param_declaration) {
    if (g_is_varargs_param(param_declaration)) {
      // varargs is the special case that marks the end of the parameter list
      break;
    }
    astn param_base_type = g_get_declaration_base_type(param_declaration);
    LLVMTypeRef t = build_convert_base_type(b, param_base_type);
    dynarray_add(arr, &t);
  }
  return arr;
}
/**
 * @brief reused in function declaration and definition 
 * because the process of LLVM function definition and declaration have the prefix same process
 * 
 * @param b 
 * @param n 
 * @return LLVMValueRef 
 */
LLVMValueRef build_function_prototype(builder b, astn n) {
  assert(g_get_function_params(n));
  sds func_name = build_symbol_name(n);
  LLVMTypeRef ret_type = build_variable_declaration_type(b, n);
  LLVMValueRef v;
  if (g_is_function_void_param(n)) {
    auto func = LLVMFunctionType(ret_type, NULL, 0, 0);
    v = LLVMAddFunction(b->module, func_name, func);
  } else {
    bool is_va = false;
    if (g_is_function_varargs(n)) {
      is_va = true;
    }
    struct dynarray params;
    dynarray_default(&params, sizeof(astn));
    build_function_parameters_type(b, g_get_function_params(n), &params);
    auto func = LLVMFunctionType(ret_type, params.data, params.used, is_va);
    v = LLVMAddFunction(b->module, func_name, func);
    dynarray_free(&params);
  }
  sdsfree(func_name);
  return v;
}

void build_function_body(builder b, astn n, LLVMValueRef v) {
  astn body = g_get_function_body(n);
  assert(body->type == ast_block);
  auto entry_block = LLVMAppendBasicBlockInContext(b->context, v, "entry");
  LLVMPositionBuilderAtEnd(b->builder, entry_block);

  astn stmt;
  slist_foreach(&body->block.list, stmt) {
    if (stmt->type == ast_declaration) {
      build_declaration(b, stmt);
    } else {
      build_expression(b, stmt);
    }
  }
  astn func_return_base_type = g_get_function_return_base_type(n);
  auto default_type = build_convert_base_type(b, func_return_base_type);
  if (func_return_base_type->ctype.type == TOK_KW_VOID) {
    LLVMBuildRetVoid(b->builder);
    return;
  }
  LLVMBuildRet(b->builder, LLVMConstNull(default_type));
}

/**
 * @brief build a declaration which maybe a function declaration/definition, variable declaration
 * If the V is not NULL, it means the declaration has been built, just return 
 * @param b builder 
 * @param n a declaration that should not be typedef storage class
 * @return typed_value  
 */
typed_value build_declaration(builder b, astn n) {
  assert(n->type == ast_declaration);
  if (n->declaration.V) {
    log_trace("declaration %s has been built", n->declaration.ident);
    return n->declaration.V;
  }
  astn decl_specs = g_get_declaration_specifier(n);
  assert(decl_specs->ctype.storage != TOK_KW_TYPEDEF);
  LLVMValueRef v;

  if (g_get_function_params(n)) {
    // function declaration or definition
    v = build_function_prototype(b, n);
    if (g_is_function_definition(n)) {
      build_function_body(b, n, v);
    }
  } else if (g_is_declaration_in_function_scope(n) &&
             decl_specs->ctype.storage != TOK_KW_EXTERN) {
    // variable in function
    v = build_alloca_variable(b, n);
  } else {
    v = build_global_variable(b, n);
  }

  // storage class setting
  switch (decl_specs->ctype.storage) {
  case TOK_KW_EXTERN:
    LLVMSetLinkage(v, LLVMExternalLinkage);
    break;
  case TOK_KW_STATIC:
    LLVMSetLinkage(v, LLVMInternalLinkage);
    break;
  case TOK_UNKNOWN:
  default:
    break;
  }
  struct slist ptr_type_chain;
  slist_copy(&ptr_type_chain, &n->declaration.type_chain);
  log_trace("add pointer type to typed value: %s",
            LLVMGetValueName2(v, &(size_t){}));
  astn ptr = ast_new(ast_ctype);
  ptr->ctype.type = '*';
  slist_add_head(&ptr_type_chain, ptr);
  return n->declaration.V = typed_value_new(v, &ptr_type_chain);
}

void build_trans_unit(builder b, slist symtab) {
  astn n;
  slist_foreach(symtab, n) {
    if (g_get_declaration_specifier(n)->ctype.storage == TOK_KW_TYPEDEF) {
      log_trace("skip the typedef declaration: %s", n->declaration.ident);
      continue;
    }
    build_declaration(b, n);
  }
}
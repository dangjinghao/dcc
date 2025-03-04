#include "ast.h"
#include "builder.h"
#include "dynarray/dynarray.h"
#include "grammar.h"
#include "lexer.h"
#include "log/log.h"
#include "macro/macro.h"
#include "sds/sds.h"
#include <assert.h>
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include <stdbool.h>
#include <stddef.h>

LLVMTypeRef build_convert_struct_type(astn n, builder b) {
  LLVMTypeRef t;
  if (n->type == ast_ref) {
    // the reference of exists struct definition
    n = n->ref;
  }
  // because the symbols order is stack style, the referenced struct type
  // may not be created.
  assert(n->type == ast_struct_union_declaration);
  if (n->struct_union_declaration.V) {
    t = n->struct_union_declaration.V;
    log_debug("refering the existed struct type:%s", LLVMGetStructName(t));
  } else {
    sds name;
    if (n->struct_union_declaration.ident) {
      name = sdscatprintf(sdsempty(), STRUCT_FMT,
                          n->struct_union_declaration.ident,
                          n->struct_union_declaration.uid);
    } else {
      // abstract struct, use uid
      name = sdscatprintf(sdsempty(), STRUCT_ABSTRACT_FMT,
                          n->struct_union_declaration.uid);
    }
    log_debug("create struct definition with name: %s", name);

    t = LLVMStructCreateNamed(b->context, name);
    sdsfree(name);
    assert(n->struct_union_declaration.V == NULL);
    n->struct_union_declaration.V = t;
    // TODO: currently, we just add an int type to this struct definition
    LLVMTypeRef elements[] = {LLVMInt32TypeInContext(b->context)};
    LLVMStructSetBody(t, elements, 1, false);
  }
  return t;
}

LLVMTypeRef build_convert_base_type(astn n, builder b) {
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
    return build_convert_struct_type(n->ctype.user_defined_type, b);
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
  return build_convert_base_type(_t, b);
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
      return sdscatprintf(sdsempty(), FUNCTION_STATIC_FMT,
                          n->declaration.scope_ref->declaration.ident,
                          n->declaration.ident, n->declaration.uid);
    } else {
      log_panic("those variables in function scope would drop their name");
    }
  } else {
    // global scope
    if (decl_specs->ctype.storage == TOK_KW_STATIC) {
      // static global variable
      return sdscatprintf(sdsempty(), GLOBAL_STATIC_FMT, n->declaration.ident,
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

dynarray build_function_parameters_type(builder b, astn params, dynarray arr) {
  assert(params->type == ast_parameters);
  astn param_declaration;
  slist_foreach(&params->parameters.list, param_declaration) {
    if (g_is_varargs_param(param_declaration)) {
      // varargs is the special case that marks the end of the parameter list
      break;
    }
    astn param_base_type = g_get_declaration_base_type(param_declaration);
    LLVMTypeRef t = build_convert_base_type(param_base_type, b);
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
  sds sym_name = build_symbol_name(n);
  LLVMTypeRef ret_type = build_variable_declaration_type(b, n);
  LLVMValueRef v;
  if (g_is_function_void_param(n)) {
    auto func = LLVMFunctionType(ret_type, NULL, 0, 0);
    v = LLVMAddFunction(b->module, sym_name, func);
  } else {
    bool is_va = false;
    if (g_is_function_varargs(n)) {
      is_va = true;
    }
    struct dynarray params;
    dynarray_default(&params, sizeof(astn));
    build_function_parameters_type(b, g_get_function_params(n), &params);
    auto func = LLVMFunctionType(ret_type, params.data, params.used, is_va);
    v = LLVMAddFunction(b->module, sym_name, func);
    dynarray_free(&params);
  }
  sdsfree(sym_name);
  return v;
}

/**
 * @brief build a declaration which maybe a function declaration/definition, variable declaration
 * 
 * @param b builder 
 * @param n a declaration that should not be typedef storage class
 * @return LLVMValueRef 
 */
LLVMValueRef build_declaration(builder b, astn n) {
  assert(n->type == ast_declaration);
  astn decl_specs = g_get_declaration_specifier(n);
  assert(decl_specs->ctype.storage != TOK_KW_TYPEDEF);
  LLVMValueRef v;

  if (g_get_function_params(n)) {
    // function declaration or definition
    v = build_function_prototype(b, n);
    if (g_is_function_definition(n)) {
      BUILDING();
    }
  } else if (g_is_declaration_in_function_scope(n)) {
    // variable in function
    BUILDING();
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
  return v;
}
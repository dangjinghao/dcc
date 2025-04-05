#include "ast.h"
#include "builder.h"
#include "dynarray/dynarray.h"
#include "grammar.h"
#include "lexer.h"
#include "log/log.h"
#include "macro/macro.h"
#include "sds/sds.h"
#include "slist/slist.h"
#include "token.h"
#include "typed_value/typed_value.h"
#include <assert.h>
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/Types.h>
#include <stdbool.h>
#include <stddef.h>

dynarray build_declaration_type_struct_member(builder b, astn n, dynarray arr) {
  assert(n->type == ast_struct_or_union_declaration);
  astn struct_member_declaration;
  slist_foreach(&n->struct_or_union_declaration.member_declarations,
                struct_member_declaration) {
    LLVMTypeRef t =
        build_declaration_variable_type(b, struct_member_declaration);
    if (struct_member_declaration->declaration.extdata) {
      log_panic("Unsupported struct member declaration with bitfield");
    }
    dynarray_add(arr, &t);
  }
  return arr;
}

dynarray build_declaration_type_union_member(builder b, astn n, dynarray arr) {
  // get the max size member type
  assert(n->type == ast_struct_or_union_declaration);
  LLVMTypeRef max_size_type = NULL;
  astn union_member_declaration;
  slist_foreach(&n->struct_or_union_declaration.member_declarations,
                union_member_declaration) {
    LLVMTypeRef t =
        build_declaration_variable_type(b, union_member_declaration);
    if (union_member_declaration->declaration.extdata) {
      log_panic("Unsupported union member declaration with bitfield");
    }
    if (max_size_type == NULL) {
      max_size_type = t;
    } else {
      size_t old_size = LLVMABISizeOfType(b->data_layout, max_size_type);
      size_t sz = LLVMABISizeOfType(b->data_layout, t);
      if (old_size < sz) {
        max_size_type = t;
      }
    }
  }
  dynarray_add(arr, &max_size_type);
  return arr;
}

LLVMTypeRef build_declaration_struct_or_union(builder b, astn n) {
  assert(n->type == ast_ctype);
  assert(g_is_struct_or_union_token(n->ctype.type));
  astn udt = n->ctype.user_defined_type;
  LLVMTypeRef t;
  if (udt->type == ast_ref) {
    udt = udt->ref;
  }
  assert(udt->type == ast_struct_or_union_declaration);
  if (udt->struct_or_union_declaration.V) {
    log_debug("reuse the existing struct definition");
    t = udt->struct_or_union_declaration.V;
  } else {
    assert(udt->type == ast_struct_or_union_declaration);
    struct dynarray dyn_elements;
    dynarray_default(&dyn_elements, sizeof(LLVMTypeRef));
    if (n->ctype.type == TOK_KW_STRUCT) {
      build_declaration_type_struct_member(b, udt, &dyn_elements);
    } else {
      build_declaration_type_union_member(b, udt, &dyn_elements);
    }
    size_t elements_count = dyn_elements.used;
    LLVMTypeRef *elements = dyn_elements.data;
    if (udt->struct_or_union_declaration.ident) {
      sds name;
      name = sdscatprintf(
          sdsempty(), n->ctype.type == TOK_KW_STRUCT ? STRUCT_FMT : UNION_FMT,
          udt->struct_or_union_declaration.ident,
          udt->struct_or_union_declaration.uid);
      log_debug("create struct/union definition with name: %s", name);
      t = LLVMStructCreateNamed(b->context, name);
      LLVMStructSetBody(t, elements, elements_count, false);
      sdsfree(name);
    } else {
      log_debug("create anonymous struct/union definition");
      t = LLVMStructTypeInContext(b->context, elements, elements_count, false);
    }
    dynarray_free(&dyn_elements);
    udt->struct_or_union_declaration.V = t;
  }
  return t;
}

/**
 * @brief build a variable declaration type,
 * if the declaration is a function declaration, return the return type
 * 
 * @param b 
 * @param n 
 * @return LLVMTypeRef 
 */
LLVMTypeRef build_declaration_variable_type(builder b, astn n) {
  assert(n->type == ast_declaration);
  astn _t = build_type_chain_get_base_type(&n->declaration.type_chain);
  if (_t->type == ast_parameters) {
    _t = build_type_chain_new_get_function_return_base_type(
        &n->declaration.type_chain);
  }
  return build_type_base_type_convert_to_llvm(b, _t);
}

/**
 * @brief generate a symbol name by the storage class and the scope
 * 
 * @param n 
 * @return char* 
 */
sds build_symbol_name(astn n) {
  assert(n->type == ast_declaration);
  if (n->declaration.storage_class == TOK_KW_EXTERN) {
    return sdsdup(parse_declaration_get_ident(n));
  } else if (n->declaration.storage_class == TOK_KW_STATIC) {
    return sdscatprintf(sdsempty(), STATIC_VAR_FMT,
                        parse_declaration_get_ident(n), n->declaration.uid);
  } else if (!g_is_declaration_in_function_scope(n)) {
    return sdsdup(parse_declaration_get_ident(n));
  }
  assert(g_is_declaration_in_function_scope(n));
  return sdscatprintf(sdsempty(), VAR_FMT, parse_declaration_get_ident(n),
                      n->declaration.uid);
}

void build_variable_global_init(LLVMValueRef pv, astn n,
                                LLVMTypeRef value_type) {
  if (!n->declaration.extdata) {
    log_debug("no initializer for %s, use default",
              LLVMGetValueName2(pv, &(size_t){}));
    LLVMSetInitializer(pv, LLVMConstNull(value_type));
  } else {
    BUILDING();
  }
}

LLVMValueRef build_variable_global(builder b, astn n) {
  assert(n->type == ast_declaration);
  sds sym_name = build_symbol_name(n);
  LLVMValueRef pv;
  if ((pv = LLVMGetNamedGlobal(b->module, sym_name))) {
    log_debug("reuse the existing global variable: %s", sym_name);
    return pv;
  }
  LLVMTypeRef value_type = build_declaration_variable_type(b, n);
  pv = LLVMAddGlobal(b->module, value_type, sym_name);
  sdsfree(sym_name);
  if (n->declaration.storage_class != TOK_KW_EXTERN) {
    build_variable_global_init(pv, n, value_type);
  }
  return pv;
}

static void build_variable_alloca_init_recurisve(builder b, LLVMValueRef pv,
                                                 astn init,
                                                 slist target_type_chain);
static void build_variable_alloca_init_recurisve_expr_init_list(
    builder b, LLVMValueRef pv, astn init, slist target_type_chain) {
  astn target_base_type = build_type_chain_get_base_type(target_type_chain);
  astn d;
  size_t i = 0;
  slist gep_target_type_chain = target_type_chain;
  slist_foreach(&init->initializer_list.list, d) {
    // GEP
    log_trace("build initializer list %zu", i);
    LLVMValueRef index =
        LLVMConstInt(LLVMInt64TypeInContext(b->context), i, false);
    LLVMValueRef gep;
    // if it is array, use gep
    // if it is struct, use struct gep
    // btw, update the gep_target_type_chain
    if (target_base_type->type == ast_ctype &&
        target_base_type->ctype.type == '[') {
      // array
      log_trace("build array initializer");
      gep_target_type_chain =
          build_type_chain_new_get_points_to_type_chian(b, target_type_chain);
      astn item_base_type =
          build_type_chain_get_base_type(gep_target_type_chain);
      gep = LLVMBuildGEP2(
          b->builder, build_type_base_type_convert_to_llvm(b, item_base_type),
          pv, &index, 1, "initrgep");

    } else if (target_base_type->type == ast_ctype &&
               (target_base_type->ctype.type == TOK_KW_STRUCT)) {
      // struct or union
      log_trace("build struct initializer");
      gep = LLVMBuildStructGEP2(
          b->builder, build_declaration_struct_or_union(b, target_base_type),
          pv, i, "initrstructgep");
      astn result;
      build_type_struct_type_get_member_by_id(target_base_type, i, &result);
      assert(result->type == ast_declaration);
      gep_target_type_chain = &result->declaration.type_chain;
    } else {
      log_panic("Unsupported this type with initializer list");
    }
    build_variable_alloca_init_recurisve(b, gep, d, gep_target_type_chain);
    i++;
  }
}

static void build_variable_alloca_init_recurisve_expr(builder b,
                                                      LLVMValueRef pv,
                                                      astn init,
                                                      slist target_type_chain) {
  astn target_base_type = build_type_chain_get_base_type(target_type_chain);
  typed_value v = build_expression(b, init);
  if (build_type_chain_is_str(&v->type_chain) &&
      target_base_type->ctype.type == '[') {
    // initializer is a string, call memcpy
    log_debug("initializer is a string, use memcpy");
    LLVMValueRef str_len =
        LLVMConstInt(LLVMInt64TypeInContext(b->context),
                     build_type_chain_string_get_len(&v->type_chain), false);
    LLVMBuildMemCpy(b->builder, pv, 1, v->v, 1, str_len);
  } else {
    typed_value ptr = typed_value_new(
        pv, build_type_chain_new_add_pointer(b, target_type_chain));
    build_value_store(b, v, ptr);
  }
}

static void build_variable_alloca_init_recurisve(builder b, LLVMValueRef pv,
                                                 astn init,
                                                 slist target_type_chain) {
  if (init->type == ast_initializer_list) {
    return build_variable_alloca_init_recurisve_expr_init_list(
        b, pv, init, target_type_chain);
  } else {
    return build_variable_alloca_init_recurisve_expr(b, pv, init,
                                                     target_type_chain);
  }
}

void build_variable_alloca_init(builder b, LLVMValueRef pv, astn n) {
  astn init = n->declaration.extdata;
  assert(init->type == ast_initializer);
  log_trace("alloca variable %s has initializer",
            LLVMGetValueName2(pv, &(size_t){}));
  if (init->initializer.init->type == ast_initializer_list) {
    astn target_base_type =
        build_type_chain_get_base_type(&n->declaration.type_chain);
    // init with ConstNull
    log_trace("initializer is a list, use memset");
    LLVMValueRef empty_value = LLVMConstNull(LLVMInt8TypeInContext(b->context));
    LLVMBuildMemSet(
        b->builder, pv, empty_value,
        LLVMSizeOf(build_type_base_type_convert_to_llvm(b, target_base_type)),
        1);
  }
  return build_variable_alloca_init_recurisve(b, pv, init->initializer.init,
                                              &n->declaration.type_chain);
}

LLVMValueRef build_variable_alloca(builder b, astn n) {
  assert(n->type == ast_declaration);
  sds sym_name = build_symbol_name(n);
  LLVMTypeRef value_type = build_declaration_variable_type(b, n);
  LLVMBasicBlockRef current_block = LLVMGetInsertBlock(b->builder);
  LLVMBasicBlockRef entry_block = LLVMGetFirstBasicBlock(b->fn);
  LLVMValueRef last_entry_inst = LLVMGetLastInstruction(entry_block);
  if (last_entry_inst && LLVMIsATerminatorInst(last_entry_inst)) {
    LLVMPositionBuilderBefore(b->builder, last_entry_inst);
  } else {
    log_trace("entry block is empty or non-terminator, skip position before "
              "the last instruction");
    LLVMPositionBuilderAtEnd(b->builder, entry_block);
  }
  auto pv = LLVMBuildAlloca(b->builder, value_type, sym_name);
  builder_set_llvm_align(b, pv, value_type);
  sdsfree(sym_name);
  LLVMPositionBuilderAtEnd(b->builder, current_block);

  // initialize the alloca variable
  if (n->declaration.extdata) {
    build_variable_alloca_init(b, pv, n);
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
    astn param_base_type = build_type_chain_get_base_type(
        &param_declaration->declaration.type_chain);
    LLVMTypeRef t;
    if (param_base_type->type == ast_ctype &&
        param_base_type->ctype.type == '[') {
      // multi array type declaration in function parameter
      // **inplace modify** the first array type to pointer type
      param_declaration->declaration.type_chain =
          *build_type_chain_inplace_cast_indexable_implict(
              b, &param_declaration->declaration.type_chain);
      param_base_type = build_type_chain_get_base_type(
          &param_declaration->declaration.type_chain);
    }
    t = build_type_base_type_convert_to_llvm(b, param_base_type);

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
  LLVMTypeRef func = build_type_declaration_function_convert_to_llvm(b, n);
  LLVMValueRef v = LLVMAddFunction(b->module, func_name, func);
  sdsfree(func_name);
  return v;
}

void build_function_body(builder b, astn n, LLVMValueRef v) {
  astn body = g_get_function_body(n);
  auto entry_block = LLVMAppendBasicBlockInContext(b->context, v, "entry");
  // Create alloca variables for function parameters
  LLVMPositionBuilderAtEnd(b->builder, entry_block);
  astn params = g_get_function_params(n);
  if (params && !g_is_function_void_param(n)) {
    astn param_decl;
    size_t param_idx = 0;
    slist_foreach(&params->parameters.list, param_decl) {
      if (g_is_varargs_param(param_decl)) {
        // Skip varargs parameter
        break;
      }
      // check whether the parameter declaration is struct or union, we don't support it right now
      astn param_base_type =
          build_type_chain_get_base_type(&param_decl->declaration.type_chain);
      if (param_base_type->type == ast_ctype &&
          g_is_struct_or_union_token(param_base_type->ctype.type)) {
        log_panic("Pass struct or union parameter by value is not supported "
                  "right now");
      }
      assert(!(param_base_type->type == ast_ctype &&
               param_base_type->ctype.type == '['));
      // Create an alloca for this parameter
      sds param_name = build_symbol_name(param_decl);
      LLVMTypeRef param_type = build_declaration_variable_type(b, param_decl);
      LLVMValueRef alloca = LLVMBuildAlloca(b->builder, param_type, param_name);

      // Store the parameter value into the alloca
      LLVMValueRef param = LLVMGetParam(v, param_idx);
      slist param_decl_type_chain = &param_decl->declaration.type_chain;
      typed_value ptr = typed_value_new(
          alloca, build_type_chain_new_add_pointer(b, param_decl_type_chain));
      build_value_store(b, typed_value_new(param, param_decl_type_chain), ptr);

      // Save the alloca as the parameter's value
      assert(param_decl->declaration.V == NULL);

      slist ptr_type_chain =
          build_type_chain_new_add_pointer(b, param_decl_type_chain);
      param_decl->declaration.V = typed_value_new(alloca, ptr_type_chain);

      sdsfree(param_name);
      param_idx++;
    }
  }
  LLVMPositionBuilderAtEnd(b->builder, entry_block);
  build_statement_block(b, body);
  astn func_return_base_type =
      build_type_chain_new_get_function_return_base_type(
          &n->declaration.type_chain);
  if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(b->builder))) {
    // the last statement is terminator
    log_debug("the last statement is terminator, skip the default return");
    return;
  }
  log_debug("add default return statement");
  if (func_return_base_type->ctype.type == TOK_KW_VOID) {
    LLVMBuildRetVoid(b->builder);
    return;
  } else {
    if (g_is_struct_or_union_token(func_return_base_type->ctype.type)) {
      log_panic("Return struct or union by value is not supported right now");
    }
    auto default_type =
        build_type_base_type_convert_to_llvm(b, func_return_base_type);
    LLVMBuildRet(b->builder, LLVMConstNull(default_type));
  }
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
    log_trace("declaration %s has been built", parse_declaration_get_ident(n));
    return n->declaration.V;
  }

  LLVMValueRef v;
  if (n->declaration.storage_class == TOK_KW_TYPEDEF) {
    log_debug("ignore the typedef declaration: %s",
              parse_declaration_get_ident(n));
    return NULL;
  } else if (g_get_function_params(n)) {
    // function declaration or definition
    v = build_function_prototype(b, n);
  } else if (n->declaration.storage_class == TOK_KW_EXTERN ||
             n->declaration.storage_class == TOK_KW_STATIC) {
    // extern variable in function
    v = build_variable_global(b, n);
  } else if (g_is_declaration_in_function_scope(n)) {
    v = build_variable_alloca(b, n);
  } else {
    v = build_variable_global(b, n);
  }
  // storage class setting
  switch (n->declaration.storage_class) {
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
  slist ptr_type_chain =
      build_type_chain_new_add_pointer(b, &n->declaration.type_chain);
  n->declaration.V = typed_value_new(v, ptr_type_chain);
  // we should build the function body after add it to n.declaration.V
  if (g_is_function_definition(n)) {
    // it may be used in defining a new function in a function scope
    LLVMValueRef prev_function = b->fn;
    LLVMBasicBlockRef prev_block = LLVMGetInsertBlock(b->builder);
    b->fn = v;
    build_function_body(b, n, v);
    builder_label_list_check_undefined(b);
    builder_label_list_free(b);
    b->fn = prev_function;
    LLVMPositionBuilderAtEnd(b->builder, prev_block);
  }
  return n->declaration.V;
}

void build_trans_unit(builder b, slist symtab) {
  astn n;
  slist_foreach(symtab, n) {
    assert(n->type == ast_declaration);
    build_declaration(b, n);
  }
}

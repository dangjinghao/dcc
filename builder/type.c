#include "ast.h"
#include "builder.h"
#include "convert/convert.h"
#include "grammar.h"
#include "lexer.h"
#include "log/log.h"
#include "macro/macro.h"
#include "slist/slist.h"
#include "token.h"
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
/**
 * @brief We have to support the ptr type, so we need typed_value to store the ptr type chain
 * 
 * @param b 
 * @param v 
 * @param type_chain 
 * @return typed_value 
 */
typed_value build_type_convert_by_type_chain(builder b, typed_value v,
                                             slist type_chain) {
  astn base_type = slist_peek_head(&v->type_chain);
  astn target_type = slist_peek_head(type_chain);
  assert(base_type->type == ast_ctype);
  assert(target_type->type == ast_ctype);

#define CONVERT_CASE(from, to, BF)                                             \
  if (base_type->ctype.type == from && target_type->ctype.type == to) {        \
    log_trace("cast " #from " to " #to);                                       \
    v->v = (BF)(b->builder, v->v,                                              \
                build_type_ctype_convert_to_llvm(b, target_type), "cast");     \
    auto nv = typed_value_new(v->v, type_chain);                               \
    return nv;                                                                 \
  }
  // int -> int
  if (g_is_int_family_tok(base_type->ctype.type) &&
      g_is_int_family_tok(target_type->ctype.type)) {
    v->v = LLVMBuildIntCast2(b->builder, v->v,
                             build_type_ctype_convert_to_llvm(b, target_type),
                             base_type->ctype.signint == TOK_KW_SIGNED, "cast");
    log_trace("cast type in int family");
    auto nv = typed_value_new(v->v, type_chain);
    return nv;
  }
  // ptr -> int
  CONVERT_CASE('*', TOK_KW_CHAR, LLVMBuildPtrToInt);
  CONVERT_CASE('*', TOK_KW_SHORT, LLVMBuildPtrToInt);
  CONVERT_CASE('*', TOK_KW_INT, LLVMBuildPtrToInt);
  CONVERT_CASE('*', TOK_KW_LONG, LLVMBuildPtrToInt);
  // int -> ptr
  CONVERT_CASE(TOK_KW_CHAR, '*', LLVMBuildIntToPtr);
  CONVERT_CASE(TOK_KW_SHORT, '*', LLVMBuildIntToPtr);
  CONVERT_CASE(TOK_KW_INT, '*', LLVMBuildIntToPtr);
  CONVERT_CASE(TOK_KW_LONG, '*', LLVMBuildIntToPtr);

  // fp -> fp
  CONVERT_CASE(TOK_KW_FLOAT, TOK_KW_DOUBLE, LLVMBuildFPCast);
  CONVERT_CASE(TOK_KW_DOUBLE, TOK_KW_FLOAT, LLVMBuildFPCast);

  CONVERT_CASE(TOK_KW_CHAR, TOK_KW_FLOAT,
               base_type->ctype.signint == TOK_KW_SIGNED ? LLVMBuildSIToFP
                                                         : LLVMBuildUIToFP);
  CONVERT_CASE(TOK_KW_CHAR, TOK_KW_DOUBLE,
               base_type->ctype.signint == TOK_KW_SIGNED ? LLVMBuildSIToFP
                                                         : LLVMBuildUIToFP);
  CONVERT_CASE(TOK_KW_SHORT, TOK_KW_FLOAT,
               base_type->ctype.signint == TOK_KW_SIGNED ? LLVMBuildSIToFP
                                                         : LLVMBuildUIToFP);
  CONVERT_CASE(TOK_KW_SHORT, TOK_KW_DOUBLE,
               base_type->ctype.signint == TOK_KW_SIGNED ? LLVMBuildSIToFP
                                                         : LLVMBuildUIToFP);
  CONVERT_CASE(TOK_KW_INT, TOK_KW_FLOAT,
               base_type->ctype.signint == TOK_KW_SIGNED ? LLVMBuildSIToFP
                                                         : LLVMBuildUIToFP);
  CONVERT_CASE(TOK_KW_INT, TOK_KW_DOUBLE,
               base_type->ctype.signint == TOK_KW_SIGNED ? LLVMBuildSIToFP
                                                         : LLVMBuildUIToFP);
  CONVERT_CASE(TOK_KW_LONG, TOK_KW_FLOAT,
               base_type->ctype.signint == TOK_KW_SIGNED ? LLVMBuildSIToFP
                                                         : LLVMBuildUIToFP);
  CONVERT_CASE(TOK_KW_LONG, TOK_KW_DOUBLE,
               base_type->ctype.signint == TOK_KW_SIGNED ? LLVMBuildSIToFP
                                                         : LLVMBuildUIToFP);
  CONVERT_CASE(TOK_KW_FLOAT, TOK_KW_CHAR,
               target_type->ctype.signint == TOK_KW_SIGNED ? LLVMBuildFPToSI
                                                           : LLVMBuildFPToUI);
  CONVERT_CASE(TOK_KW_DOUBLE, TOK_KW_CHAR,
               target_type->ctype.signint == TOK_KW_SIGNED ? LLVMBuildFPToSI
                                                           : LLVMBuildFPToUI);
  CONVERT_CASE(TOK_KW_FLOAT, TOK_KW_SHORT,
               target_type->ctype.signint == TOK_KW_SIGNED ? LLVMBuildFPToSI
                                                           : LLVMBuildFPToUI);
  CONVERT_CASE(TOK_KW_DOUBLE, TOK_KW_SHORT,
               target_type->ctype.signint == TOK_KW_SIGNED ? LLVMBuildFPToSI
                                                           : LLVMBuildFPToUI);
  CONVERT_CASE(TOK_KW_FLOAT, TOK_KW_INT,
               target_type->ctype.signint == TOK_KW_SIGNED ? LLVMBuildFPToSI
                                                           : LLVMBuildFPToUI);
  CONVERT_CASE(TOK_KW_DOUBLE, TOK_KW_INT,
               target_type->ctype.signint == TOK_KW_SIGNED ? LLVMBuildFPToSI
                                                           : LLVMBuildFPToUI);
  CONVERT_CASE(TOK_KW_FLOAT, TOK_KW_LONG,
               target_type->ctype.signint == TOK_KW_SIGNED ? LLVMBuildFPToSI
                                                           : LLVMBuildFPToUI);
  CONVERT_CASE(TOK_KW_DOUBLE, TOK_KW_LONG,
               target_type->ctype.signint == TOK_KW_SIGNED ? LLVMBuildFPToSI
                                                           : LLVMBuildFPToUI);

#undef CONVERT_CASE

  log_trace("no need to cast, but we still copy type chain to process ptr "
            "type cast");
  v = typed_value_new(v->v, type_chain);

  return v;
}

/**
 * @brief 
 * 
 * @param type TOK_LIT_* 
 * @return slist 
 */
slist build_type_chain_by_lit(enum tok_type type) {
  slist type_chain = calloc(1, sizeof(struct slist));
  slist_init(type_chain);
  astn base_type = ast_new(ast_ctype);
  base_type->ctype.signint = TOK_KW_SIGNED;

  switch (type) {
  case TOK_LIT_INT:
    base_type->ctype.type = TOK_KW_INT;
    break;
  case TOK_LIT_UINT:
    base_type->ctype.type = TOK_KW_INT;
    base_type->ctype.signint = TOK_KW_UNSIGNED;
    break;
  case TOK_LIT_LONG:
    base_type->ctype.type = TOK_KW_LONG;
    break;
  case TOK_LIT_ULONG:
    base_type->ctype.type = TOK_KW_LONG;
    base_type->ctype.signint = TOK_KW_UNSIGNED;
    break;
  case TOK_LIT_FLOAT:
    base_type->ctype.type = TOK_KW_FLOAT;
    break;
  case TOK_LIT_DOUBLE:
    base_type->ctype.type = TOK_KW_DOUBLE;
    break;
  case TOK_LIT_CHAR:
    base_type->ctype.type = TOK_KW_CHAR;
    break;
  case TOK_LIT_STRING:
    BUILDING();
    break;
  default:
    log_panic("Unexpected literal token");
    break;
  }
  slist_add_tail(type_chain, base_type);
  return type_chain;
}

slist build_type_chain_expr_primary(astn n) {
  assert(n->type == ast_expr_primary);
  return build_type_chain_by_lit(n->primary.type);
}

/**
 * @brief char,unsigned char,short,unsigned short,int,unsigned int,long,unsigned long,float,double
 * @param lhs 
 * @param rhs 
 * @return int -1: lhs < rhs, 0: lhs == rhs, 1: lhs > rhs
 */
int build_type_compare_promote_level(astn lhs_base_type, astn rhs_base_type) {
  assert(lhs_base_type->type == ast_ctype);
  assert(rhs_base_type->type == ast_ctype);
  static const int type_promote_level[] = {
      [TOK_KW_CHAR - __TOK_KW_START] = 0,  [TOK_KW_SHORT - __TOK_KW_START] = 1,
      [TOK_KW_INT - __TOK_KW_START] = 2,   [TOK_KW_LONG - __TOK_KW_START] = 3,
      [TOK_KW_FLOAT - __TOK_KW_START] = 4, [TOK_KW_DOUBLE - __TOK_KW_START] = 5,
  };
  if (lhs_base_type->ctype.type == '*' && rhs_base_type->ctype.type == '*') {
    return 0;
  }
  assert(g_is_numeric_tok(lhs_base_type->ctype.type));
  assert(g_is_numeric_tok(rhs_base_type->ctype.type));

  int lhs_level =
      type_promote_level[lhs_base_type->ctype.type - __TOK_KW_START];
  int rhs_level =
      type_promote_level[rhs_base_type->ctype.type - __TOK_KW_START];
  if (lhs_level < rhs_level) {
    return -1;
  } else if (lhs_level > rhs_level) {
    return 1;
  } else if (lhs_base_type->ctype.signint == TOK_KW_SIGNED &&
             rhs_base_type->ctype.signint == TOK_KW_UNSIGNED) {
    return -1;
  } else if (lhs_base_type->ctype.signint == TOK_KW_UNSIGNED &&
             rhs_base_type->ctype.signint == TOK_KW_SIGNED) {
    return 1;
  }
  return 0; // Default case
}
/**
 * @brief Special case: cast ptr to unsigned long
 * 
 * @param b 
 * @param values 
 * @return typed_value* 
 */
typed_value *build_type_2_values_type_upper_cast(builder b,
                                                 typed_value *values) {
  slist lhs_type_chain = &values[0]->type_chain;
  slist rhs_type_chain = &values[1]->type_chain;
  astn lhs_ty = slist_peek_head(lhs_type_chain);
  astn rhs_ty = slist_peek_head(rhs_type_chain);

  int cmp = build_type_compare_promote_level(lhs_ty, rhs_ty);
  if (cmp == 0) {
    return values;
  }
  if (cmp == -1) {
    log_trace("converting: lhs type < rhs type");
    values[0] = build_type_convert_by_type_chain(b, values[0], rhs_type_chain);
  } else if (cmp == 1) {
    log_trace("converting: lhs type > rhs type");
    values[1] = build_type_convert_by_type_chain(b, values[1], lhs_type_chain);
  } else {
    log_panic("Invalid type cast");
  }
  return values;
}

slist build_type_chain_copy(slist type_chain) {
  slist new_type_chain = calloc(1, sizeof(struct slist));
  slist_copy(new_type_chain, type_chain);
  return new_type_chain;
}

slist build_type_get_points_to_type_chian(builder b, slist type_chain) {
  slist points_to_type_chain = build_type_chain_copy(type_chain);
  astn ptr = slist_pop_head(points_to_type_chain);
  assert(ptr->ctype.type == '*');
  return points_to_type_chain;
}

slist build_type_function_return_type_chain(builder b, astn n) {
  assert(n->type == ast_declaration);
  slist return_type_chain = build_type_chain_copy(&n->declaration.type_chain);
  astn params = slist_pop_head(return_type_chain);
  assert(params->type == ast_parameters);
  return return_type_chain;
}

slist build_type_chain_add_pointer(builder b, slist type_chain) {
  slist new_type_chain = build_type_chain_copy(type_chain);
  astn ptr = ast_new(ast_ctype);
  ptr->ctype.type = '*';
  slist_add_head(new_type_chain, ptr);
  return new_type_chain;
}

int build_type_struct_type_get_member(astn struct_type, sds name,
                                      astn *result) {
  assert(struct_type->type == ast_ctype);
  assert(g_is_struct_or_union_token(struct_type->ctype.type));
  astn struct_declaration = struct_type->ctype.user_defined_type;
  if (struct_declaration->type == ast_ref) {
    struct_declaration = struct_declaration->ref;
  }
  assert(struct_declaration->type == ast_struct_union_declaration);
  slist members =
      &struct_declaration->struct_union_declaration.member_declarations;
  int index = 0;
  astn member;
  slist_foreach(members, member) {
    assert(member->type == ast_declaration);
    log_trace("looking for member: %s, got: %s", name,
              member->declaration.ident);
    if (member->declaration.ident &&
        sdscmp(member->declaration.ident, name) == 0) {
      *result = member;
      return index;
    }
    index += 1;
  }
  return -1;
}

LLVMTypeRef build_type_declaration_function_convert_to_llvm(builder b, astn n) {
  LLVMTypeRef func;
  LLVMTypeRef ret_type = build_declaration_variable_type(b, n);
  if (g_is_function_void_param(n)) {
    func = LLVMFunctionType(ret_type, NULL, 0, 0);
  } else {
    bool is_va = false;
    if (g_is_function_varargs(n)) {
      is_va = true;
    }
    struct dynarray params_type;
    dynarray_default(&params_type, sizeof(LLVMTypeRef));
    build_function_parameters_type(b, g_get_function_params(n), &params_type);
    func =
        LLVMFunctionType(ret_type, params_type.data, params_type.used, is_va);
    dynarray_free(&params_type);
  }
  return func;
}

LLVMTypeRef build_type_ctype_convert_to_llvm(builder b, astn n) {
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
  case TOK_KW_ENUM:
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
  case TOK_KW_UNION:
  case TOK_KW_STRUCT:
    return build_declaration_struct_or_union(b, n);
  default:
    break;
  }

  log_panic("Unsupported base type:%s", convert_repr_token(t));
  return NULL;
}

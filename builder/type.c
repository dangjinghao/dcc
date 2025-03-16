#include "ast.h"
#include "builder.h"
#include "convert/convert.h"
#include "grammar.h"
#include "lexer.h"
#include "log/log.h"
#include "macro/macro.h"
#include "slist/slist.h"
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
typed_value build_type_convert_to(builder b, typed_value v, slist type_chain) {
  astn base_type = slist_peek_head(&v->type_chain);
  astn target_type = slist_peek_head(type_chain);
  assert(base_type->type == ast_ctype);
  assert(target_type->type == ast_ctype);
  if (build_type_compare_promote_level(base_type, target_type) == 0) {
    log_trace("no need to cast");
    return v;
  }
#define CONVERT_CASE(from, to, BF)                                             \
  if (base_type->ctype.type == from && target_type->ctype.type == to) {        \
    log_trace("cast " #from " to " #to);                                       \
    v->v = (BF)(b->builder, v->v, build_convert_base_type(b, target_type),     \
                "cast");                                                       \
    v->type_chain = *type_chain;                                               \
    return v;                                                                  \
  }
  // int -> int
  if (g_is_int_family_tok(base_type->ctype.type) &&
      g_is_int_family_tok(target_type->ctype.type)) {
    v->v = LLVMBuildIntCast2(b->builder, v->v,
                             build_convert_base_type(b, target_type),
                             base_type->ctype.signint == TOK_KW_SIGNED, "cast");
    log_trace("cast type in int family");
    v->type_chain = *type_chain;
    return v;
  }
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

  log_panic("Invalid type cast:%s -> %s",
            convert_repr_token(base_type->ctype.type),
            convert_repr_token(target_type->ctype.type));
  return v;
}
/**
 * @brief 
 * 
 * @param type TOK_LIT_* 
 * @return slist 
 */
slist build_base_type_chain_by_lit(enum tok_type type) {
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
  return build_base_type_chain_by_lit(n->primary.type);
}

/**
 * @brief char,unsigned char,short,unsigned short,int,unsigned int,long,unsigned long,float,double
 * 
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
  if (cmp < 0) {
    log_trace("converting: lhs type < rhs type");
    values[0] = build_type_convert_to(b, values[0], rhs_type_chain);
  } else {
    log_trace("converting: lhs type > rhs type");
    values[1] = build_type_convert_to(b, values[1], lhs_type_chain);
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

slist build_function_return_type_chain(builder b, astn n) {
  assert(n->type == ast_declaration);
  slist return_type_chain = build_type_chain_copy(&n->declaration.type_chain);
  astn params = slist_pop_head(return_type_chain);
  assert(params->type == ast_parameters);
  return return_type_chain;
}
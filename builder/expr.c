#include "ast.h"
#include "builder.h"
#include "lexer.h"
#include "macro/macro.h"
#include "slist/slist.h"
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>

llvm_typed_value build_binop_expr(builder b, astn n) {
  switch (n->binop.op) {
  case ',': {
    build_expression(b, n->binop.lhs);
    return build_expression(b, n->binop.rhs);
  }
  }
  BUILDING();
}

llvm_typed_value build_unary(builder b, astn n) {
  if (n->unary.postfix) {
    switch (n->unary.op) {
    case '+':
    case TOK_SYM_SELF_INC:
    case TOK_SYM_SELF_DEC:
    case TOK_KW_SIZEOF:
    case '-':
    case '!':
    case '~':
    case '*':
    case '&':
      break;
    }
  } else {
    switch (n->unary.op) {
    case TOK_SYM_SELF_INC:
    case TOK_SYM_SELF_DEC:
    case TOK_SYM_ARROW:
    case '[':
    case '(':
    case '.':
      break;
    }
  }
  BUILDING();
}

slist build_primary_type_chain(astn n) {
  assert(n->type == ast_expr_primary);
  slist type_chain = calloc(1, sizeof(struct slist));
  slist_init(type_chain);
  astn base_type = ast_new(ast_ctype);
  switch (n->primary.type) {
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
  default:
    BUILDING();
    break;
  }
  slist_add_tail(type_chain, base_type);
  return type_chain;
}

llvm_typed_value build_primary(builder b, astn n) {
  switch (n->primary.type) {
  case TOK_LIT_INT: {
    return llvm_typed_value_new(LLVMConstInt(LLVMInt32TypeInContext(b->context),
                                             n->primary.v._int, true),
                                build_primary_type_chain(n));
  }
  case TOK_LIT_UINT: {
    return llvm_typed_value_new(LLVMConstInt(LLVMInt32TypeInContext(b->context),
                                             n->primary.v._uint, false),
                                build_primary_type_chain(n));
  }
  case TOK_LIT_LONG: {
    return llvm_typed_value_new(LLVMConstInt(LLVMInt64TypeInContext(b->context),
                                             n->primary.v._int, true),
                                build_primary_type_chain(n));
  }
  case TOK_LIT_ULONG: {
    return llvm_typed_value_new(LLVMConstInt(LLVMInt64TypeInContext(b->context),
                                             n->primary.v._int, false),
                                build_primary_type_chain(n));
  }
  case TOK_LIT_FLOAT:
    return llvm_typed_value_new(
        LLVMConstReal(LLVMFloatTypeInContext(b->context), n->primary.v._float),
        build_primary_type_chain(n));
  case TOK_LIT_DOUBLE:
    return llvm_typed_value_new(
        LLVMConstReal(LLVMDoubleTypeInContext(b->context),
                      n->primary.v._double),
        build_primary_type_chain(n));
  case TOK_LIT_CHAR:
    return llvm_typed_value_new(LLVMConstInt(LLVMInt8TypeInContext(b->context),
                                             n->primary.v._char, true),
                                build_primary_type_chain(n));
  case TOK_LIT_STRING:
  default:
    BUILDING();
  }
}

llvm_typed_value build_expression(builder b, astn n) {
  switch (n->type) {
  case ast_expr_binop: {
    return build_binop_expr(b, n);
  }
  case ast_expr_unary: {
    return build_unary(b, n);
  }
  case ast_expr_primary: {
    return build_primary(b, n);
  }
  default:
  }
  BUILDING();
}

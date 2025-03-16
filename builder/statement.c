
#include "ast.h"
#include "builder.h"
#include "grammar.h"
#include "log/log.h"
#include "typed_value/typed_value.h"
#include <llvm-c-19/llvm-c/Core.h>

void build_statement_jump_return(builder b, astn n) {
  astn func = n->jump_statement.scope_ref;
  assert(func->type == ast_declaration);
  slist func_type_chain = build_function_return_type_chain(b, func);
  astn func_return_type = slist_peek_head(func_type_chain);
  if (g_is_empty_statement(n->jump_statement.expr) &&
      func_return_type->ctype.type != TOK_KW_VOID) {
    log_panic("return statement with no value in non-void function");
  } else if (g_is_empty_statement(n->jump_statement.expr) &&
             func_return_type->ctype.type == TOK_KW_VOID) {
    LLVMBuildRetVoid(b->builder);
    return;
  }
  typed_value ret_val = build_expression(b, n->jump_statement.expr);
  log_trace("try to cast return value to function return type");
  typed_value ret_val_casted =
      build_type_convert_to(b, ret_val, func_type_chain);
  LLVMBuildRet(b->builder, ret_val_casted->v);
}

void build_statement_jump(builder b, astn n) {
  assert(n->type == ast_statement_jump);
  switch (n->jump_statement.type) {
  case TOK_KW_GOTO:
    break;
  case TOK_KW_RETURN:
    build_statement_jump_return(b, n);
    return;
  case TOK_KW_CONTINUE:
    break;
  case TOK_KW_BREAK:
    break;
  default:
    break;
  }
  log_panic("unknown jump statement type");
}

void build_block(builder b, astn blk) {
  assert(blk->type == ast_block);
  astn item;
  static enum ast_type statement_flag[] = {ast_statement_labeled,
                                           ast_statement_jump, ast_block};
  slist_foreach(&blk->block.list, item) {
    if (item->type == ast_declaration) {
      build_declaration(b, item);
    } else if (ARRAY_IN(statement_flag, item->type, EQ_EQ)) {
      build_statement(b, item);
    } else {
      build_expression(b, item);
    }
  }
}

void build_statement(builder b, astn n) {
  switch (n->type) {
  case ast_statement_jump:
    build_statement_jump(b, n);
    return;
  case ast_block:
    build_block(b, n);
    return;
  case ast_statement_labeled:
  case ast_statement_iteration:
  default:
    break;
  }
  BUILDING();
}
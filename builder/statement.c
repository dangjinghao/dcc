
#include "ast.h"
#include "builder.h"
#include "convert/convert.h"
#include "grammar.h"
#include "log/log.h"
#include "macro/macro.h"
#include "token.h"
#include "typed_value/typed_value.h"
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>

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

void build_statement_jump_goto(builder b, astn n) {
  assert(n->type == ast_statement_jump);
  assert(n->jump_statement.type == TOK_KW_GOTO);
  assert(n->jump_statement.expr->type == ast_ident);
  label l = builder_label_find(b, n->jump_statement.expr->ident);
  if (!l) {
    l = builder_label_new(b, n->jump_statement.expr->ident);
  }
  LLVMBuildBr(b->builder, l->block);
  // after goto
  // we would not use the `n->jump_statement.scope_ref`
  // just keep the symmetries between goto and label
  LLVMBasicBlockRef after_goto =
      LLVMAppendBasicBlockInContext(b->context, b->fn, "after_goto");
  LLVMPositionBuilderAtEnd(b->builder, after_goto);
}

void build_statement_jump(builder b, astn n) {
  assert(n->type == ast_statement_jump);
  switch (n->jump_statement.type) {
  case TOK_KW_GOTO:
    build_statement_jump_goto(b, n);
    return;
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
  log_panic("unknown jump statement type: %s",
            convert_repr_token(n->jump_statement.type));
}

void build_block(builder b, astn blk) {
  assert(blk->type == ast_block);
  astn item;
  slist_foreach(&blk->block.list, item) {
    if (item->type == ast_declaration) {
      build_declaration(b, item);
    } else {
      build_statement(b, item);
    }
  }
}

void build_statement_labeled_goto(builder b, astn n) {
  assert(n->type == ast_statement_labeled);
  assert(n->labeled_statement.type == TOK_IDENT);
  astn ident = n->labeled_statement.label_value;
  assert(ident->type == ast_ident);
  label l = builder_label_find(b, ident->ident);
  if (l) {
    if (l->defined) {
      log_panic("label %s already defined", ident->ident);
    }
  } else {
    l = builder_label_new(b, ident->ident);
  }
  l->defined = true;
  if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(b->builder))) {
    log_trace("insert block has terminator, skip building br to label %s",
              ident->ident);
  } else {
    LLVMBuildBr(b->builder, l->block);
  }
  LLVMPositionBuilderAtEnd(b->builder, l->block);
  build_statement(b, n->labeled_statement.stmt);
}

void build_statement_labeled(builder b, astn n) {
  assert(n->type == ast_statement_labeled);
  switch (n->labeled_statement.type) {
  case TOK_IDENT:
    return build_statement_labeled_goto(b, n);
  case TOK_KW_CASE:
  case TOK_KW_DEFAULT:
    BUILDING();
  default:
    break;
  }
}

void build_statement_iteration(builder b, astn n) {
  assert(n->type == ast_statement_iteration);
  switch (n->iteration.type) {
  case TOK_KW_WHILE:
    BUILDING();
  case TOK_KW_DO:
    BUILDING();
  case TOK_KW_FOR:
    BUILDING();
  default:
    break;
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
    build_statement_labeled(b, n);
    return;
  case ast_statement_iteration:
    build_statement_iteration(b, n);
    return;
  default:
    build_expression(b, n);
    break;
  }
}
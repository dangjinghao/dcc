
#include "ast.h"
#include "builder.h"
#include "convert/convert.h"
#include "dynarray/dynarray.h"
#include "grammar.h"
#include "log/log.h"
#include "sds/sds.h"
#include "slist/slist.h"
#include "token.h"
#include "typed_value/typed_value.h"
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include <stdlib.h>

void build_statement_jump_return(builder b, astn n) {
  astn func = n->jump_statement.scope_ref;
  assert(func->type == ast_declaration);
  LLVMBasicBlockRef after_return =
      LLVMAppendBasicBlockInContext(b->context, b->fn, "after_return");
  slist func_type_chain = build_function_return_type_chain(b, func);
  astn func_return_type = slist_peek_head(func_type_chain);
  if ((!n->jump_statement.expr) &&
      func_return_type->ctype.type != TOK_KW_VOID) {
    log_panic("return statement with no value in non-void function");
  } else if ((!n->jump_statement.expr) &&
             func_return_type->ctype.type == TOK_KW_VOID) {
    LLVMBuildRetVoid(b->builder);
  } else {
    // n->jump_statement.expr exists
    typed_value ret_val = build_expression(b, n->jump_statement.expr);
    astn ret_val_base_type = slist_peek_head(&ret_val->type_chain);
    if (ret_val_base_type->ctype.type == TOK_KW_VOID) {
      log_trace("return a function call with void return type");
      LLVMBuildRetVoid(b->builder);
    } else {
      log_trace("try to cast return value to function return type");
      typed_value ret_val_casted =
          build_type_convert_to(b, ret_val, func_type_chain);
      LLVMBuildRet(b->builder, ret_val_casted->v);
    }
  }
  LLVMPositionBuilderAtEnd(b->builder, after_return);
}

void build_statement_jump_goto(builder b, astn n) {
  assert(n->type == ast_statement_jump);
  assert(n->jump_statement.type == TOK_KW_GOTO);
  assert(n->jump_statement.expr);
  assert(n->jump_statement.expr->type == ast_ident);
  goto_label l = builder_label_find(b, n->jump_statement.expr->ident);
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

static void build_statement_jump_target(builder b, astn n,
                                        LLVMBasicBlockRef target_block,
                                        const char *err_msg,
                                        const char *suffix) {
  if (!target_block) {
    log_panic("%s", err_msg);
  }
  LLVMBuildBr(b->builder, target_block);
  sds block_name = sdsempty();
  block_name = sdscatprintf(block_name, "after_%s", suffix);
  LLVMBasicBlockRef after_block =
      LLVMAppendBasicBlockInContext(b->context, b->fn, block_name);
  sdsfree(block_name);
  LLVMPositionBuilderAtEnd(b->builder, after_block);
}

void build_statement_jump_break(builder b, astn n) {
  assert(n->type == ast_statement_jump);
  assert(n->jump_statement.type == TOK_KW_BREAK);
  if (n->jump_statement.scope_ref->type == ast_statement_iteration) {
    build_statement_jump_target(
        b, n, n->jump_statement.scope_ref->iteration.break_block,
        "break statement not in loop or switch", "break");
  } else if (n->jump_statement.scope_ref->type == ast_statement_switch) {
    build_statement_jump_target(
        b, n, n->jump_statement.scope_ref->_switch.break_block,
        "break statement not in switch", "switch_break");
  }
}

void build_statement_jump_continue(builder b, astn n) {
  assert(n->type == ast_statement_jump);
  assert(n->jump_statement.type == TOK_KW_CONTINUE);
  assert(n->jump_statement.scope_ref->type == ast_statement_iteration);
  build_statement_jump_target(
      b, n, n->jump_statement.scope_ref->iteration.continue_block,
      "continue statement not in loop", "continue");
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
    build_statement_jump_continue(b, n);
    return;
  case TOK_KW_BREAK:
    build_statement_jump_break(b, n);
    return;
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
  goto_label l = builder_label_find(b, ident->ident);
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

void build_statement_labeled_case(builder b, astn n) {
  assert(n->type == ast_statement_labeled);
  assert(n->labeled_statement.type == TOK_KW_CASE);
  assert(n->labeled_statement.label_value);
  long v = n->labeled_statement.label_value->primary.v._int;
  sds block_name = sdsempty();
  block_name = sdscatprintf(block_name, CASE_BLK_FMT, v);
  LLVMBasicBlockRef case_block =
      LLVMAppendBasicBlockInContext(b->context, b->fn, block_name);
  sdsfree(block_name);
  n->labeled_statement.start_block = case_block;
  LLVMBuildBr(b->builder, case_block);
  LLVMPositionBuilderAtEnd(b->builder, case_block);
  build_statement(b, n->labeled_statement.stmt);
}

void build_statement_labeled_default(builder b, astn n) {
  assert(n->type == ast_statement_labeled);
  assert(n->labeled_statement.type == TOK_KW_DEFAULT);
  LLVMBasicBlockRef default_block =
      LLVMAppendBasicBlockInContext(b->context, b->fn, "case_default");
  n->labeled_statement.start_block = default_block;
  LLVMBuildBr(b->builder, default_block);
  LLVMPositionBuilderAtEnd(b->builder, default_block);
  build_statement(b, n->labeled_statement.stmt);
}

void build_statement_labeled(builder b, astn n) {
  assert(n->type == ast_statement_labeled);
  switch (n->labeled_statement.type) {
  case TOK_IDENT:
    return build_statement_labeled_goto(b, n);
  case TOK_KW_CASE:
    return build_statement_labeled_case(b, n);
  case TOK_KW_DEFAULT:
    return build_statement_labeled_default(b, n);
  default:
    break;
  }
}

void build_statement_iteration_while(builder b, astn n) {
  assert(n->type == ast_statement_iteration);
  assert(n->iteration.type == TOK_KW_WHILE);
  LLVMBasicBlockRef cond =
      LLVMAppendBasicBlockInContext(b->context, b->fn, "while_cond");
  LLVMBasicBlockRef body =
      LLVMAppendBasicBlockInContext(b->context, b->fn, "while_body");
  LLVMBasicBlockRef after =
      LLVMAppendBasicBlockInContext(b->context, b->fn, "while_after");
  n->iteration.break_block = after;
  n->iteration.continue_block = cond;
  LLVMBuildBr(b->builder, cond);
  LLVMPositionBuilderAtEnd(b->builder, cond);
  typed_value cond_val = build_expression(b, n->iteration.cond);
  LLVMValueRef cond_test = build_value_ne0(b, cond_val);
  LLVMBuildCondBr(b->builder, cond_test, body, after);
  LLVMPositionBuilderAtEnd(b->builder, body);
  build_statement(b, n->iteration.body);
  LLVMBuildBr(b->builder, cond);
  LLVMPositionBuilderAtEnd(b->builder, after);
}

void build_statement_iteration_do(builder b, astn n) {
  assert(n->type == ast_statement_iteration);
  assert(n->iteration.type == TOK_KW_DO);
  LLVMBasicBlockRef cond =
      LLVMAppendBasicBlockInContext(b->context, b->fn, "do_cond");
  LLVMBasicBlockRef body =
      LLVMAppendBasicBlockInContext(b->context, b->fn, "do_body");
  LLVMBasicBlockRef after =
      LLVMAppendBasicBlockInContext(b->context, b->fn, "do_after");
  n->iteration.break_block = after;
  n->iteration.continue_block = cond;
  LLVMBuildBr(b->builder, body);
  LLVMPositionBuilderAtEnd(b->builder, body);
  build_statement(b, n->iteration.body);
  LLVMBuildBr(b->builder, cond);
  LLVMPositionBuilderAtEnd(b->builder, cond);
  typed_value cond_val = build_expression(b, n->iteration.cond);
  LLVMValueRef cond_test = build_value_ne0(b, cond_val);
  LLVMBuildCondBr(b->builder, cond_test, body, after);
  LLVMPositionBuilderAtEnd(b->builder, after);
}

void build_statement_iteration_for(builder b, astn n) {
  assert(n->type == ast_statement_iteration);
  assert(n->iteration.type == TOK_KW_FOR);
  LLVMBasicBlockRef init =
      LLVMAppendBasicBlockInContext(b->context, b->fn, "for_init");
  LLVMBasicBlockRef cond =
      LLVMAppendBasicBlockInContext(b->context, b->fn, "for_cond");
  LLVMBasicBlockRef inc =
      LLVMAppendBasicBlockInContext(b->context, b->fn, "for_inc");
  LLVMBasicBlockRef body =
      LLVMAppendBasicBlockInContext(b->context, b->fn, "for_body");
  LLVMBasicBlockRef after =
      LLVMAppendBasicBlockInContext(b->context, b->fn, "for_after");
  n->iteration.break_block = after;
  n->iteration.continue_block = inc;
  LLVMBuildBr(b->builder, init);
  LLVMPositionBuilderAtEnd(b->builder, init);
  if (n->iteration.init) {
    build_statement(b, n->iteration.init);
  }
  LLVMBuildBr(b->builder, cond);
  LLVMPositionBuilderAtEnd(b->builder, cond);
  if (n->iteration.cond) {
    typed_value cond_val = build_expression(b, n->iteration.cond);
    LLVMValueRef cond_test = build_value_ne0(b, cond_val);
    LLVMBuildCondBr(b->builder, cond_test, body, after);
  } else {
    LLVMBuildBr(b->builder, body);
  }
  LLVMPositionBuilderAtEnd(b->builder, body);
  build_statement(b, n->iteration.body);
  // just for readability
  LLVMBuildBr(b->builder, inc);
  LLVMPositionBuilderAtEnd(b->builder, inc);
  if (n->iteration.inc) {
    build_statement(b, n->iteration.inc);
  }
  LLVMBuildBr(b->builder, cond);
  LLVMPositionBuilderAtEnd(b->builder, after);
}

void build_statement_iteration(builder b, astn n) {
  assert(n->type == ast_statement_iteration);
  switch (n->iteration.type) {
  case TOK_KW_WHILE:
    build_statement_iteration_while(b, n);
    return;
  case TOK_KW_DO:
    build_statement_iteration_do(b, n);
    return;
  case TOK_KW_FOR:
    build_statement_iteration_for(b, n);
    return;
  default:
    break;
  }
}

void build_statement_if(builder b, astn n) {
  assert(n->type == ast_statement_if);
  LLVMBasicBlockRef then_block =
      LLVMAppendBasicBlockInContext(b->context, b->fn, "if_then");
  LLVMBasicBlockRef after_block =
      LLVMAppendBasicBlockInContext(b->context, b->fn, "if_after");
  typed_value cond_val = build_expression(b, n->_if.cond);
  LLVMValueRef cond_test = build_value_ne0(b, cond_val);
  LLVMBasicBlockRef else_block =
      n->_if._f ? LLVMAppendBasicBlockInContext(b->context, b->fn, "if_else")
                : after_block;

  LLVMBuildCondBr(b->builder, cond_test, then_block, else_block);
  LLVMPositionBuilderAtEnd(b->builder, then_block);
  build_statement(b, n->_if._t);
  LLVMBuildBr(b->builder, after_block);

  if (n->_if._f) {
    LLVMPositionBuilderAtEnd(b->builder, else_block);
    build_statement(b, n->_if._f);
    LLVMBuildBr(b->builder, after_block);
  }

  LLVMPositionBuilderAtEnd(b->builder, after_block);
}

static int build_case_ref_cmp(const void *a, const void *b) {
  astn *case_ref_a = (astn *)a;
  astn *case_ref_b = (astn *)b;
  assert((*case_ref_a)->labeled_statement.label_value->type ==
         ast_expr_primary);
  assert((*case_ref_b)->labeled_statement.label_value->type ==
         ast_expr_primary);
  long case_v_a = (*case_ref_a)->labeled_statement.label_value->primary.v._int;
  long case_v_b = (*case_ref_b)->labeled_statement.label_value->primary.v._int;
  return case_v_a - case_v_b;
}

void build_statement_switch_allocate_algo(builder b, astn n,
                                          LLVMBasicBlockRef switch_after) {
  typed_value cond_val = build_expression(b, n->_switch.cond);
  astn cond_val_base_type = slist_peek_head(&cond_val->type_chain);
  astn *case_ref;
  // sort case_refs, prepare for binary search
  qsort(n->_switch.case_refs.data, n->_switch.case_refs.used,
        n->_switch.case_refs.item_size, build_case_ref_cmp);
  dynarray_foreach(&n->_switch.case_refs, case_ref) {
    assert((*case_ref)->labeled_statement.label_value->type ==
           ast_expr_primary);
    long case_v = (*case_ref)->labeled_statement.label_value->primary.v._int;
    LLVMValueRef case_cmp = LLVMBuildICmp(
        b->builder, LLVMIntEQ, cond_val->v,
        LLVMConstInt(build_convert_base_type(b, cond_val_base_type), case_v, 1),
        "case_cmp");
    LLVMBasicBlockRef case_test_after =
        LLVMAppendBasicBlockInContext(b->context, b->fn, "case_test_after");
    LLVMBuildCondBr(b->builder, case_cmp,
                    (*case_ref)->labeled_statement.start_block,
                    case_test_after);
    LLVMPositionBuilderAtEnd(b->builder, case_test_after);
  }
  astn _default = n->_switch.default_ref;
  if (_default) {
    LLVMBuildBr(b->builder, _default->labeled_statement.start_block);
  } else {
    LLVMBuildBr(b->builder, switch_after);
  }
}

void build_statement_switch(builder b, astn n) {
  LLVMBasicBlockRef switch_body =
      LLVMAppendBasicBlockInContext(b->context, b->fn, "switch_body");
  LLVMBasicBlockRef switch_after =
      LLVMAppendBasicBlockInContext(b->context, b->fn, "switch_after");
  LLVMBasicBlockRef switch_cond =
      LLVMAppendBasicBlockInContext(b->context, b->fn, "switch_cond");
  LLVMBuildBr(b->builder, switch_cond);
  n->_switch.break_block = switch_after; // set break block for break in switch
  // create switch body first to avoid getting empty basic block in cond block processing
  LLVMPositionBuilderAtEnd(b->builder, switch_body);
  build_statement(b, n->_switch.body);
  LLVMBuildBr(b->builder, switch_after);
  LLVMPositionBuilderAtEnd(b->builder, switch_cond);
  // implement cond test and br in there
  build_statement_switch_allocate_algo(b, n, switch_after);
  LLVMPositionBuilderAtEnd(b->builder, switch_after);
}
void build_statement(builder b, astn n) {
  assert(n);
  if (g_is_empty_statement(n)) {
    // do nothing
    log_trace("got an empty statement, do nothing");
    return;
  }
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
  case ast_statement_if:
    build_statement_if(b, n);
    return;
  case ast_statement_switch:
    build_statement_switch(b, n);
    return;
  default:
    build_expression(b, n);
    break;
  }
}

#include "ast.h"
#include "dynarray/dynarray.h"
#include "grammar.h"
#include "lexer.h"
#include "log/log.h"
#include "parser.h"
#include "slist/slist.h"
#include "token.h"

astn parse_statement_expression(parser p) {
  assert(g_is_expression_statement_firstset(p));
  // move the empty statement check to the parse_statement
  astn expr;
  if (p->current_token == ';') {
    expr = g_new_empty_statement();
  } else {
    expr = parse_expression(p);
  }
  parser_consume_with(p, ';');
  return expr;
}

/**
 * 
 * @param parser 
 * @return astn 
 */
astn parse_statement_compound(parser parser) {
  assert(g_is_compound_statement_firstset(parser));
  parser_consume_with(parser, '{');
  astn block = ast_new(ast_block);
  while (true) {
    if (g_is_external_declaration_firstset(parser)) {
      parse_external_declaration(parser, &block->block.list);
    } else if (g_is_statement_firstset(parser)) {
      slist_add_tail(&block->block.list, parse_statement(parser));
    } else {
      break;
    }
  }
  parser_consume_with(parser, '}');
  return block;
}
astn parse_statement_selection(parser p) {
  assert(g_is_selection_statement_firstset(p));
  astn sel;
  if (p->current_token == TOK_KW_IF) {
    parser_consume(p);
    sel = ast_new(ast_statement_if);
    parser_consume_with(p, '(');
    sel->_if.cond = parse_expression(p);
    parser_consume_with(p, ')');
    sel->_if._t = parse_statement(p);
    if (p->current_token == TOK_KW_ELSE) {
      parser_consume(p);
      sel->_if._f = parse_statement(p);
    } else {
      sel->_if._f = NULL;
    }
  } else {
    parser_consume(p);
    sel = ast_new(ast_statement_switch);
    parser_consume_with(p, '(');
    sel->_switch.cond = parse_expression(p);
    parser_consume_with(p, ')');
    astn prev_switch_scope = p->switch_scope;
    astn prev_break_scope = p->break_scope;
    p->break_scope = p->switch_scope = sel;
    sel->_switch.body = parse_statement(p);
    p->switch_scope = prev_switch_scope;
    p->break_scope = prev_break_scope;
  }
  return sel;
}

astn parse_statement_iteration(parser p) {
  assert(g_is_iteration_statement_firstset(p));
  astn iter = ast_new(ast_statement_iteration);
  iter->iteration.type = p->current_token;
  parser_consume(p);
  astn prev_break_scope = p->break_scope;
  astn prev_continue_scope = p->continue_scope;
  p->continue_scope = p->break_scope = iter;
  switch (iter->iteration.type) {
  case TOK_KW_WHILE: {
    parser_consume_with(p, '(');
    iter->iteration.cond = parse_expression(p);
    parser_consume_with(p, ')');
    iter->iteration.body = parse_statement(p);
    break;
  }
  case TOK_KW_DO: {
    iter->iteration.body = parse_statement(p);
    parser_consume_with(p, TOK_KW_WHILE);
    parser_consume_with(p, '(');
    iter->iteration.cond = parse_expression(p);
    parser_consume_with(p, ')');
    parser_consume_with(p, ';');
    break;
  }
  case TOK_KW_FOR: {
    parser_consume_with(p, '(');
    parser_push_scope(p);
    if (p->current_token == ';') {
      iter->iteration.init = NULL;
      parser_consume(p);
    } else if (g_is_declaration_firstset(p)) {
      astn decl = ast_new(ast_block);
      parse_external_declaration(p, &decl->block.list);
      iter->iteration.init = decl;
      // ';' is consumed
    } else {
      // expr
      iter->iteration.init = parse_expression(p);
      parser_consume_with(p, ';');
    }
    if (p->current_token == ';') {
      iter->iteration.cond = NULL;
    } else {
      iter->iteration.cond = parse_expression(p);
    }
    parser_consume(p);
    if (p->current_token != ')') {
      iter->iteration.inc = parse_expression(p);
    } else {
      iter->iteration.inc = NULL;
    }
    parser_consume_with(p, ')');
    iter->iteration.body = parse_statement(p);
    parser_pop_scope(p);
    break;
  }
  default:
    break;
  }
  p->break_scope = prev_break_scope;
  p->continue_scope = prev_continue_scope;
  return iter;
}

astn parse_statement_jump(parser p) {
  assert(g_is_jump_statement_firstset(p));
  astn jump = ast_new(ast_statement_jump);
  jump->jump_statement.type = p->current_token;
  parser_consume(p);

  switch (jump->jump_statement.type) {
  case TOK_KW_GOTO:
    jump->jump_statement.expr = parse_expr_ident(p);
    // it would not be used in the current implementation
    jump->jump_statement.scope_ref = NULL;
    break;
  case TOK_KW_CONTINUE:
    jump->jump_statement.scope_ref = p->continue_scope;
    assert(jump->jump_statement.scope_ref);
    break;
  case TOK_KW_BREAK:
    jump->jump_statement.scope_ref = p->break_scope;
    assert(jump->jump_statement.scope_ref);
    break;
  case TOK_KW_RETURN:
    if (p->current_token == ';') {
      jump->jump_statement.expr = NULL;
    } else {
      jump->jump_statement.expr = parse_expression(p);
    }
    jump->jump_statement.scope_ref = p->function_scope;
    assert(jump->jump_statement.scope_ref);
    break;
  default:
  }
  parser_consume_with(p, ';');
  return jump;
}

astn parse_statement(parser p) {
  assert(g_is_statement_firstset(p));
  astn stmt = NULL;
  if (g_is_labeled_statement_firstset(p)) {
    // special case for the expression stats with ident
    if (!(stmt = parse_statement_labeled(p))) {
      log_debug("Failed to parse labeled statement,retrying to parse "
                "expression statement");
      stmt = parse_statement_expression(p);
    }
  } else if (g_is_compound_statement_firstset(p)) {
    parser_push_scope(p);
    stmt = parse_statement_compound(p);
    parser_pop_scope(p);
  } else if (g_is_selection_statement_firstset(p)) {
    stmt = parse_statement_selection(p);
  } else if (g_is_iteration_statement_firstset(p)) {
    stmt = parse_statement_iteration(p);
  } else if (g_is_jump_statement_firstset(p)) {
    stmt = parse_statement_jump(p);
  } else {
    stmt = parse_statement_expression(p);
  }
  return stmt;
}

static bool parse_statement_labeled_case_check_duplicate(parser p, astn label) {
  assert(label->type == ast_expr_primary);
  assert(label->primary.type == TOK_LIT_LONG);
  astn *prev;
  dynarray_foreach(&p->switch_scope->_switch.case_refs, prev) {
    assert((*prev)->type == ast_statement_labeled);
    assert((*prev)->labeled_statement.type == TOK_KW_CASE);
    assert((*prev)->labeled_statement.label_value->type == ast_expr_primary);
    if ((*prev)->labeled_statement.label_value->primary.v._int ==
        label->primary.v._int) {
      return true;
    }
  }
  return false;
}

astn parse_statement_labeled(parser p) {
  assert(g_is_labeled_statement_firstset(p));
  // goto label has the same first set as normal expression statement,
  // we can use snapshot
  astn label = NULL;
  enum tok_type label_type = p->current_token;
  switch (p->current_token) {
  case TOK_IDENT: {
    struct parser backup;
    parser_new_snapshot(&backup, p);
    label = parse_expr_ident(p);
    if (p->current_token == ':') {
      parser_consume(p);
      parser_destory(&backup);
    } else {
      // failed to parse label, restore parser state
      ast_free(label);
      parser_restore(p, &backup);
      return NULL;
    }
    break;
  }
  case TOK_KW_CASE: {
    parser_consume(p);
    label = parse_expr_const_int(p);
    long v = parser_expr_eval_const_int(label);
    ast_free(label);
    label = ast_new(ast_expr_primary);
    label->primary.type = TOK_LIT_LONG;
    label->primary.v._int = v;
    parser_consume_with(p, ':');
    break;
  }
  case TOK_KW_DEFAULT: {
    parser_consume(p);
    parser_consume_with(p, ':');
    break;
  }
  }
  astn stmt;
  if (!g_is_statement_firstset(p)) {
    log_trace("got an empty statement in labeled statement parsing process");
    // C99 feature: labeled empty statement
    // just add an empty statement
    stmt = g_new_empty_statement();
  } else {
    stmt = parse_statement(p);
  }
  astn ls = ast_new(ast_statement_labeled);
  ls->labeled_statement.type = label_type;
  ls->labeled_statement.label_value = label;
  ls->labeled_statement.stmt = stmt;
  if (label_type == TOK_KW_CASE) {
    // check the case value is unique
    if (parse_statement_labeled_case_check_duplicate(p, label)) {
      compiler_error(p->lexer, "duplicate case label:%ld",
                     label->primary.v._int);
    }
    // add to switch scope
    if (!p->switch_scope) {
      compiler_error(p->lexer, "case label not in switch statement");
    }
    log_trace("add case label:%ld", label->primary.v._int);
    dynarray_add(&p->switch_scope->_switch.case_refs, &ls);
  } else if (label_type == TOK_KW_DEFAULT) {
    if (!p->switch_scope) {
      compiler_error(p->lexer, "default label not in switch statement");
    } else if (p->switch_scope->_switch.default_ref) {
      compiler_error(p->lexer, "duplicate default label");
    }
    log_trace("setting default label");
    p->switch_scope->_switch.default_ref = ls;
  }
  return ls;
}

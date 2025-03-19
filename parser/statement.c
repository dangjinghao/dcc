
#include "ast.h"
#include "grammar.h"
#include "lexer.h"
#include "log/log.h"
#include "macro/macro.h"
#include "parser.h"
#include "slist/slist.h"

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
astn parse_statement_selection(parser p) { BUILDING(); }

astn parse_statement_iteration(parser p) {
  assert(g_is_iteration_statement_firstset(p));
  astn iter = ast_new(ast_statement_iteration);
  iter->iteration.type = p->current_token;
  parser_consume(p);
  astn prev_scope = p->break_scope;
  p->break_scope = iter;
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
    if (g_is_declaration_firstset(p)) {
      astn decl = ast_new(ast_block);
      parse_external_declaration(p, &decl->block.list);
      iter->iteration.init = decl;
    } else {
      // statement_expression supports empty statement, so it will be used in the init and cond
      iter->iteration.init = parse_statement_expression(p);
    }
    // ';' is consumed by the parse_statement_expression
    iter->iteration.cond = parse_statement_expression(p);
    if (p->current_token != ')') {
      iter->iteration.inc = parse_expression(p);
    } else {
      iter->iteration.inc = g_new_empty_statement();
    }
    parser_consume_with(p, ')');
    iter->iteration.body = parse_statement(p);
    parser_pop_scope(p);
    break;
  }
  default:
    break;
  }
  p->break_scope = prev_scope;
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
  case TOK_KW_BREAK:
    // TODO: special case for break in switch
    jump->jump_statement.scope_ref = p->break_scope;
    assert(jump->jump_statement.scope_ref);
    break;
  case TOK_KW_RETURN:
    if (p->current_token == ';') {
      jump->jump_statement.expr = g_new_empty_statement();
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

astn parse_statement_labeled(parser p) {
  assert(g_is_labeled_statement_firstset(p));
  // goto label has the same first set as normal expression statement,
  // we can use snapshot
  astn label = NULL;
  astn scope_ref = NULL;
  enum tok_type label_type = p->current_token;
  switch (p->current_token) {
  case TOK_IDENT: {
    struct parser backup;
    parser_new_snapshot(&backup, p);
    label = parse_expr_ident(p);
    if (p->current_token == ':') {
      parser_consume(p);
      parser_destory(&backup);
      break;
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
    parser_consume_with(p, ':');
    scope_ref = p->switch_scope;
    break;
  }
  case TOK_KW_DEFAULT: {
    parser_consume(p);
    parser_consume_with(p, ':');
    scope_ref = p->switch_scope;
    break;
  }
  }
  astn stmt = parse_statement(p);
  astn ls = ast_new(ast_statement_labeled);
  ls->labeled_statement.type = label_type;
  ls->labeled_statement.label_value = label;
  ls->labeled_statement.stmt = stmt;
  ls->labeled_statement.scope_ref = scope_ref;
  return ls;
}


#include "ast.h"
#include "grammar.h"
#include "lexer.h"
#include "log/log.h"
#include "macro/macro.h"
#include "parser.h"
#include "slist/slist.h"

astn parse_expression_statement(parser p) {
  assert(g_is_expression_statement_firstset(p));
  // move the empty statement check to the parse_statement
  astn expr;
  if (p->current_token == ';') {
    expr = g_create_empty_statement();
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
astn parse_compound_statement(parser parser) {
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
astn parse_selection_statement(parser p) { BUILDING(); }
astn parse_iteration_statement(parser p) { BUILDING(); }
astn parse_jump_statement(parser p) {
  assert(g_is_jump_statement_firstset(p));
  astn jump = ast_new(ast_jump_statement);
  switch (p->current_token) {
  case TOK_KW_GOTO:
    jump->jump_statement.type = TOK_KW_GOTO;
    parser_consume(p);
    jump->jump_statement.expr = parse_ident(p);
    break;
  case TOK_KW_CONTINUE:
  case TOK_KW_BREAK:
    jump->jump_statement.type = p->current_token;
    parser_consume(p);
    jump->jump_statement.target_block_ref = p->interruptable_scope;
    assert(jump->jump_statement.target_block_ref);
    break;
  case TOK_KW_RETURN:
    jump->jump_statement.type = TOK_KW_RETURN;
    parser_consume(p);
    if (g_is_expression_firstset(p)) {
      jump->jump_statement.expr = parse_expression(p);
    }
    jump->jump_statement.target_block_ref = p->current_function_scope;
    assert(jump->jump_statement.target_block_ref);
    break;
  }
  parser_consume_with(p, ';');
  return jump;
}

astn parse_statement(parser p) {
  assert(g_is_statement_firstset(p));
  astn stmt = NULL;
  if (g_is_labeled_statement_firstset(p)) {
    // special case for the expression stats with ident
    if (!(stmt = parse_labeled_statement(p))) {
      log_debug("Failed to parse labeled statement,retrying to parse "
                "expression statement");
      stmt = parse_expression_statement(p);
    }
  } else if (g_is_compound_statement_firstset(p)) {
    parser_push_scope(p);
    stmt = parse_compound_statement(p);
    parser_pop_scope(p);
  } else if (g_is_selection_statement_firstset(p)) {
    stmt = parse_selection_statement(p);
  } else if (g_is_iteration_statement_firstset(p)) {
    stmt = parse_iteration_statement(p);
  } else if (g_is_jump_statement_firstset(p)) {
    stmt = parse_jump_statement(p);
  } else {
    stmt = parse_expression_statement(p);
  }
  return stmt;
}

astn parse_labeled_statement(parser p) {
  assert(g_is_labeled_statement_firstset(p));
  // goto label has the same first set as normal expression statement,
  // we can use snapshot
  astn label = NULL;
  enum tok_type label_type = p->current_token;
  switch (p->current_token) {
  case TOK_IDENT: {
    struct parser backup;
    parser_snapshot(&backup, p);
    label = parse_ident(p);
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
    BUILDING();
    break;
  }
  case TOK_KW_DEFAULT: {
    BUILDING();
    break;
  }
  }
  astn stmt = parse_statement(p);
  astn ls = ast_new(ast_labeled_statement);
  ls->labeled_statement.type = label_type;
  ls->labeled_statement.label_value = label;
  ls->labeled_statement.stmt = stmt;

  return ls;
}

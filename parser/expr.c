
#include "ast.h"
#include "lexer.h"
#include "macro/macro.h"
#include "parser.h"
#include <assert.h>
#include <stdbool.h>

void parser_from_lexer(struct parser *parser, struct lexer *lexer) {
  parser->lexer = lexer;
  parser_consume(parser);
}

ast_node_ptr parse_ident(struct parser *parser) {
  ast_node_ptr node = ast_new(ast_ident);
  node->ident = parser->lexer->lex_token._ident;
  parser_consume(parser);
  return node;
}

ast_node_ptr parse_primary_expr(struct parser *parser) {
  // TODO: support (expr)
  ast_node_ptr node = ast_new(ast_expr_value);
  assert((parser->current_token > __TOK_LIT_START &&
          parser->current_token < __TOK_LIT_END) ||
         parser->current_token == ast_ident);
  node->value.type = parser->current_token;
  node->value.v = parser->lexer->lex_token;
  parser_consume(parser);
  return node;
}

// static ast_node_ptr parse_unary_advanced_postfix(struct parser *parser) {
// }

ast_node_ptr parse_unary_postfix(struct parser *parser) {
  static int postfix_ops[] = {
      TOK_SYM_SELF_INC, TOK_SYM_SELF_DEC, TOK_SYM_ARROW, '[', '(', '.'};
  ast_node_ptr v = parse_primary_expr(parser);
  while (true) {
    if (ARRAY_IN(postfix_ops, parser->current_token, EQ_EQ)) {
      ast_node_ptr node = ast_new(ast_expr_unary);
      // TODO: support advanced postfix
      node->unary.op = parser->current_token;
      node->unary.postfix = true;
      parser_consume(parser);
      node->unary.expr = v;
      v = node;
    } else {
      break;
    }
  }
  return v;
}

ast_node_ptr parse_unary_prefix(struct parser *parser) {
  static int prefix_ops[] = {
      TOK_SYM_SELF_INC,
      TOK_SYM_SELF_DEC,
      TOK_KW_SIZEOF,
      '-',
      '+',
      '!',
      '~',
      '*',
      '&',
  };
  if (ARRAY_IN(prefix_ops, parser->current_token, EQ_EQ)) {
    ast_node_ptr node = ast_new(ast_expr_unary);
    node->unary.op = parser->current_token;
    node->unary.postfix = false;
    parser_consume(parser);
    node->unary.expr = parse_unary_postfix(parser);
    return node;
  }

  return parse_unary_postfix(parser);
}

/* Pratt parser */

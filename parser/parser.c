#include "parser.h"
#include "lexer.h"
int parser_consume(struct parser *parser) {
  return parser->current_token = lexer_next_token(parser->lexer);
}

void parser_from_lexer(struct parser *parser, struct lexer *lexer) {
  parser->lexer = lexer;
  parser_consume(parser);
}

int parser_consume_with(struct parser *parser, int token) {
  if (parser->current_token == token) {
    if (token == TOK_LIT_STRING || token == TOK_IDENT) {
      sdsfree(parser->lexer->lex_token._str);
    }
    return parser_consume(parser);
  }
  compiler_error(parser->lexer, "Expected token %s, got %s",
                 token_string(token), token_string(parser->current_token));
  return 0;
}

void parser_free_ast(astn node) {
  if (!node)
    return;
  if (node->type == ast_expr_unary) {
    parser_free_ast(node->unary.expr);
  } else if (node->type == ast_expr_binop) {
    parser_free_ast(node->binop.lhs);
    parser_free_ast(node->binop.rhs);
  } else if (node->type == ast_expr_ternary) {
    parser_free_ast(node->ternary.cond);
    parser_free_ast(node->ternary._t);
    parser_free_ast(node->ternary._f);
  } else if (node->type == ast_expr_primary) {
    if (node->primary.type == TOK_LIT_STRING || node->primary.type == TOK_IDENT) {
      sdsfree(node->primary.v._str);
    }
  }

  free(node);
}
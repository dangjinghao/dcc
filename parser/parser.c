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
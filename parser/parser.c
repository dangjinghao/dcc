#include "parser.h"
int parser_consume(struct parser *parser) {
  return parser->current_token = lexer_next_token(parser->lexer);
}

void parser_eat(struct parser *parser, int tok) {
  if (parser->current_token == tok) {
    switch (tok) {
    case TOK_LIT_STRING:
      sdsfree(parser->lexer->lex_token._str);
      break;
    case TOK_IDENT:
      sdsfree(parser->lexer->lex_token._ident);
      break;
    }
    parser_consume(parser);
  } else {
    compiler_error(parser->lexer, "Unexpected token: %s, expected: %s",
                   token_string(parser->current_token), token_string(tok));
  }
}
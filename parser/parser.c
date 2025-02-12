#include "parser.h"
int parser_consume(struct parser *parser) {
  return parser->current_token = lexer_next_token(parser->lexer);
}

void parser_from_lexer(struct parser *parser, struct lexer *lexer) {
  parser->lexer = lexer;
  parser_consume(parser);
}

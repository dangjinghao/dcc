#include "lexer.h"
#include "log/log.h"
#include "sds/sds.h"
#include <stdio.h>

void process(struct lexer *lexer) {

  int tok;
  while ((tok = lexer_next_token(lexer)) != TOK_EOF) {
    char *X = lexer_get_token_str_in_token_table(tok, NULL);
    if (X)
      printf("<%s>\n", X);
    else if (tok == TOK_IDENT) {
      printf("<id-%s>\n", lex_token._ident);
      sdsfree(lex_token._ident);
    } else if (tok < 256) {
      printf("<sym-%c>\n", tok);
    } else if (tok == TOK_LIT_INT) {
      printf("<%ldi>\n", lex_token._int);
    } else if (tok == TOK_LIT_UINT) {
      printf("<%luu>\n", lex_token._uint);
    } else if (tok == TOK_LIT_LONG) {
      printf("<%ldl>\n", lex_token._int);
    } else if (tok == TOK_LIT_ULONG) {
      printf("<%luul>\n", lex_token._uint);
    } else if (tok == TOK_LIT_FLOAT) {
      printf("<%ff>\n", lex_token._float);
    } else if (tok == TOK_LIT_DOUBLE) {
      printf("<%fd>\n", lex_token._double);
    } else if (tok == TOK_LIT_CHAR) {
      printf("<char-%c-%d>\n", lex_token._char, lex_token._char);
    } else if (tok == TOK_LIT_STRING) {
      printf("<str-%s>\n", lex_token._str);
      sdsfree(lex_token._str);
    }
  }
}

char *str_char1 = "\"XXX\\n\" '\\66' '\\0'";

char *str1 = "'\\''";
int file_main() {
  struct lexer lexer;
  log_set_level(LOG_LEVEL_DEBUG);
  lexer_from_file(&lexer, "lexer/lexer.c");
  process(&lexer);
  lexer_destroy(&lexer);

  return 0;
}

int str_char_main() {
  struct lexer lexer;
  lexer_from_string(&lexer, str_char1);
  process(&lexer);
  lexer_destroy(&lexer);

  return 0;
}

int stdin_main() {
  struct lexer lexer;
  lexer_from_fp(&lexer, stdin);
  process(&lexer);
  lexer_destroy(&lexer);
  return 0;
}

int main() { return str_char_main(); }
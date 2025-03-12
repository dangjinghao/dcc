#ifndef LEXER_H
#define LEXER_H
#include "log/log.h"
#include "sds/sds.h"
#include "token.h" // IWYU pragma: export
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

struct lexer {
  size_t ln;
  sds src;
  size_t pos;
  union token {
    char _char;
    double _double;
    float _float;
    unsigned long _uint;
    long _int;
    sds _str;
    sds _ident;
  } lex_token;
  char filename[256];
};

void lexer_new_from_string(struct lexer *lexer, char *src);
void lexer_new_from_file(struct lexer *lexer, char *filename);
void lexer_new_from_fp(struct lexer *lexer, FILE *fp);
void lexer_destroy(struct lexer *lexer);
void lexer_snapshot_new(struct lexer *dst, struct lexer *src);
bool lexer_is_eof(struct lexer *lexer);
int lexer_peek(struct lexer *lexer);
void lexer_eat(struct lexer *lexer, char c);
bool lexer_try_eat_str(struct lexer *lexer, char *str);
void lexer_skip_whitespace(struct lexer *lexer);
int lexer_get_next_token(struct lexer *lexer);

#define compiler_error(lexer, fmt, ...)                                        \
  do {                                                                         \
    log_panic("Error at %s:%zu: " fmt, (lexer)->filename, (lexer)->ln,         \
              ##__VA_ARGS__);                                                  \
  } while (0)
#endif
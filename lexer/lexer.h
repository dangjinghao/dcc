#ifndef LEXER_H
#define LEXER_H
#include "log/log.h"
#include "sds/sds.h"
#include <stdbool.h>
#include <stdlib.h>

#include <stdio.h>
enum tok_type {
  TOK_EOF = -1,
  TOK_UNKNOWN = 0,
  // the range of char [0, 255] is reserved for ASCII characters
  TOK_IDENT = 256,

  __TOK_KW_START,
  TOK_KW_AUTO,
  TOK_KW_BREAK,
  TOK_KW_CASE,
  TOK_KW_CHAR,
  TOK_KW_CONST,
  TOK_KW_CONTINUE,
  TOK_KW_DEFAULT,
  TOK_KW_DO,
  TOK_KW_DOUBLE,
  TOK_KW_ELSE,
  TOK_KW_ENUM,
  TOK_KW_EXTERN,
  TOK_KW_FLOAT,
  TOK_KW_FOR,
  TOK_KW_GOTO,
  TOK_KW_IF,
  TOK_KW_INT,
  TOK_KW_LONG,
  TOK_KW_REGISTER,
  TOK_KW_RETURN,
  TOK_KW_SHORT,
  TOK_KW_SIGNED,
  TOK_KW_SIZEOF,
  TOK_KW_STATIC,
  TOK_KW_STRUCT,
  TOK_KW_SWITCH,
  TOK_KW_TYPEDEF,
  TOK_KW_UNION,
  TOK_KW_UNSIGNED,
  TOK_KW_VOID,
  TOK_KW_VOLATILE,
  TOK_KW_WHILE,
  TOK_KW_VARARGS,
  __TOK_KW_END,

  __TOK_LIT_START,
  TOK_LIT_INT,
  TOK_LIT_UINT,
  TOK_LIT_LONG,
  TOK_LIT_ULONG,
  TOK_LIT_FLOAT,
  TOK_LIT_DOUBLE,
  TOK_LIT_CHAR,
  TOK_LIT_STRING,
  __TOK_LIT_END,

  TOK_SYM_LEQ,
  TOK_SYM_GEQ,
  TOK_SYM_EQ,
  TOK_SYM_NEQ,
  TOK_SYM_RSHIFT,
  TOK_SYM_LSHIFT,
  TOK_SYM_SELF_ADD,
  TOK_SYM_SELF_SUB,
  TOK_SYM_SELF_MUL,
  TOK_SYM_SELF_DIV,
  TOK_SYM_SELF_MOD,
  TOK_SYM_SELF_BIT_OR,
  TOK_SYM_SELF_BIT_XOR,
  TOK_SYM_SELF_BIT_AND,
  TOK_SYM_SELF_RSHIFT,
  TOK_SYM_SELF_LSHIFT,
  TOK_SYM_SELF_INC,
  TOK_SYM_SELF_DEC,
  TOK_SYM_ARROW,
  TOK_SYM_LOGIC_OR,
  TOK_SYM_LOGIC_AND,

};

struct lexer {
  size_t ln;
  char filename[FILENAME_MAX];
  sds src;
  union token {
    char _char;
    double _double;
    float _float;
    unsigned long _uint;
    long _int;
    sds _str;
    sds _ident;
  } lex_token;
  size_t pos;
};

void lexer_from_string(struct lexer *lexer, char *src);
void lexer_from_file(struct lexer *lexer, char *filename);
void lexer_from_fp(struct lexer *lexer, FILE *fp);
void lexer_destroy(struct lexer *lexer);
void lexer_copy(struct lexer *dst, struct lexer *src);
void lexer_snapshot(struct lexer *dst, struct lexer *src);
bool lexer_eof(struct lexer *lexer);
int lexer_peek(struct lexer *lexer);
void lexer_eat(struct lexer *lexer, char c);
bool lexer_try_eat_str(struct lexer *lexer, char *str);
void lexer_skip_whitespace(struct lexer *lexer);
struct token_table_entry {
  char *str;
  enum tok_type type;
};
enum tok_type
lexer_get_token_type_in_token_table(char *str, struct token_table_entry *table);
char *lexer_get_token_str_in_token_table(enum tok_type type,
                                         struct token_table_entry *table);
char *token_string(int type);
int lexer_next_token(struct lexer *lexer);
#define compiler_error(lexer, fmt, ...)                                        \
  do {                                                                         \
    log_error("Error at %s:%zu: " fmt, (lexer)->filename, (lexer)->ln,         \
              ##__VA_ARGS__);                                                  \
    exit(1);                                                                   \
  } while (0)
#endif
#ifndef LEXER_TOKEN_H
#define LEXER_TOKEN_H

#include <stddef.h>
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

  TOK_SYM_VARARGS,
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

struct lexer_token_table_entry {
  char *str;
  enum tok_type type;
};
extern struct lexer_token_table_entry lexer_token_kw_table[],
    lexer_token_multi_char_sym_table[];
enum tok_type lexer_token_get_token_in(char *str,
                                       struct lexer_token_table_entry *table);
char *lexer_token_get_str_in(enum tok_type type,
                             struct lexer_token_table_entry *table);
char *lexer_token_get_str(int type);
size_t lexer_token_get_sizeof(int t);
#endif
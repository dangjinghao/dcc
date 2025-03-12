#include "token.h"
#include "convert/convert.h"
#include <string.h>

struct lexer_token_table_entry lexer_token_kw_table[] = {
    {.str = "auto", .type = TOK_KW_AUTO},
    {.str = "break", .type = TOK_KW_BREAK},
    {.str = "case", .type = TOK_KW_CASE},
    {.str = "char", .type = TOK_KW_CHAR},
    {.str = "const", .type = TOK_KW_CONST},
    {.str = "continue", .type = TOK_KW_CONTINUE},
    {.str = "default", .type = TOK_KW_DEFAULT},
    {.str = "do", .type = TOK_KW_DO},
    {.str = "double", .type = TOK_KW_DOUBLE},
    {.str = "else", .type = TOK_KW_ELSE},
    {.str = "enum", .type = TOK_KW_ENUM},
    {.str = "extern", .type = TOK_KW_EXTERN},
    {.str = "float", .type = TOK_KW_FLOAT},
    {.str = "for", .type = TOK_KW_FOR},
    {.str = "goto", .type = TOK_KW_GOTO},
    {.str = "if", .type = TOK_KW_IF},
    {.str = "int", .type = TOK_KW_INT},
    {.str = "long", .type = TOK_KW_LONG},
    {.str = "register", .type = TOK_KW_REGISTER},
    {.str = "return", .type = TOK_KW_RETURN},
    {.str = "short", .type = TOK_KW_SHORT},
    {.str = "signed", .type = TOK_KW_SIGNED},
    {.str = "sizeof", .type = TOK_KW_SIZEOF},
    {.str = "static", .type = TOK_KW_STATIC},
    {.str = "struct", .type = TOK_KW_STRUCT},
    {.str = "switch", .type = TOK_KW_SWITCH},
    {.str = "typedef", .type = TOK_KW_TYPEDEF},
    {.str = "union", .type = TOK_KW_UNION},
    {.str = "unsigned", .type = TOK_KW_UNSIGNED},
    {.str = "void", .type = TOK_KW_VOID},
    {.str = "volatile", .type = TOK_KW_VOLATILE},
    {.str = "while", .type = TOK_KW_WHILE},
    {NULL, TOK_UNKNOWN},
};

struct lexer_token_table_entry lexer_token_multi_char_sym_table[] = {
    {.str = "...", .type = TOK_SYM_VARARGS}, // high token level
    {.str = "<=", .type = TOK_SYM_LEQ},
    {.str = ">=", .type = TOK_SYM_GEQ},
    {.str = "==", .type = TOK_SYM_EQ},
    {.str = "!=", .type = TOK_SYM_NEQ},
    {.str = ">>=", .type = TOK_SYM_SELF_RSHIFT},
    {.str = ">>", .type = TOK_SYM_RSHIFT},
    {.str = "<<=", .type = TOK_SYM_SELF_LSHIFT},
    {.str = "<<", .type = TOK_SYM_LSHIFT},
    {.str = "+=", .type = TOK_SYM_SELF_ADD},
    {.str = "-=", .type = TOK_SYM_SELF_SUB},
    {.str = "*=", .type = TOK_SYM_SELF_MUL},
    {.str = "/=", .type = TOK_SYM_SELF_DIV},
    {.str = "%=", .type = TOK_SYM_SELF_MOD},
    {.str = "|=", .type = TOK_SYM_SELF_BIT_OR},
    {.str = "^=", .type = TOK_SYM_SELF_BIT_XOR},
    {.str = "&=", .type = TOK_SYM_SELF_BIT_AND},
    {.str = "++", .type = TOK_SYM_SELF_INC},
    {.str = "--", .type = TOK_SYM_SELF_DEC},
    {.str = "->", .type = TOK_SYM_ARROW},
    {.str = "||", .type = TOK_SYM_LOGIC_OR},
    {.str = "&&", .type = TOK_SYM_LOGIC_AND}, // low token level
    {NULL, TOK_UNKNOWN},
};

enum tok_type
lexer_token_get_token_in(char *str,
                              struct lexer_token_table_entry *table) {
  while (table->str) {
    if (strcmp(str, table->str) == 0) {
      return table->type;
    }
    table++;
  }
  return TOK_UNKNOWN;
}

char *lexer_token_get_str_in(enum tok_type type,
                             struct lexer_token_table_entry *table) {
  if (table == NULL) {
    table = lexer_token_multi_char_sym_table;
    while (table->str) {
      if (type == table->type) {
        return table->str;
      }
      table++;
    }
    table = lexer_token_kw_table;
    while (table->str) {
      if (type == table->type) {
        return table->str;
      }
      table++;
    }
    return convert_repr_token(type);
  } else {
    while (table->str) {
      if (type == table->type) {
        return table->str;
      }
      table++;
    }
  }
  return NULL;
}

char *lexer_token_get_str(int type) {
  static char buf[16] = {0};
  if (type == TOK_EOF) {
    return "EOF";
  } else if (type < 256) {
    memset(buf, 0, sizeof(buf));
    buf[0] = type;
    return buf;
  }
  return lexer_token_get_str_in(type, NULL);
}

size_t lexer_token_get_sizeof(enum tok_type t) {
  switch (t) {
  case TOK_KW_INT:
    return sizeof(int);
  case TOK_KW_VOID:
  case TOK_KW_CHAR:
    return sizeof(char);
  case TOK_KW_FLOAT:
    return sizeof(float);
  case TOK_KW_DOUBLE:
    return sizeof(double);
  case TOK_KW_LONG:
    return sizeof(long);
  case TOK_KW_SHORT:
    return sizeof(short);
  default:
    log_panic("unsupported type: %s", convert_repr_token(t));
  }
  return 0;
}
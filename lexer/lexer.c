#include "lexer.h"
#include "log/log.h"
#include "macro/macro.h"
#include "sds/sds.h"
#include <assert.h>
#include <convert/convert.h>
#include <ctype.h>
#include <errno.h> // IWYU pragma: keep
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void lexer_from_string(struct lexer *lexer, char *src) {
  lexer->ln = 1;
  lexer->pos = 0;
  lexer->src = sdsnew(src);
  strcpy(lexer->filename, "<src>");
}

void lexer_from_fp(struct lexer *lexer, FILE *fp) {
  log_debug("Reading file from fp");
  assert(fp);
  lexer->ln = 1;
  lexer->pos = 0;
  lexer->src = sdsempty();
  char file_buf[4096];
  while (!feof(fp)) {
    size_t n = fread(file_buf, sizeof(char), sizeof(file_buf) - 1, fp);
    log_debug("Read %d bytes", n);
    file_buf[n] = '\0';
    lexer->src = sdscat(lexer->src, file_buf);
  }
  strcpy(lexer->filename, "<src>");
}

void lexer_from_file(struct lexer *lexer, char *filename) {
  log_debug("Open file: %s", filename);
  FILE *f = fopen(filename, "r");
  if (!f) {
    log_panic("Failed to open file %s: %s", filename, strerror(errno));
  }
  lexer_from_fp(lexer, f);
  strncpy(lexer->filename, filename, CONST_STRLEN(lexer->filename));
  fclose(f);
}

void lexer_destroy(struct lexer *lexer) {
  sdsfree(lexer->src);
  memset(lexer, 0, sizeof(*lexer));
}

void lexer_snapshot(struct lexer *dst, struct lexer *src) { *dst = *src; }

bool lexer_eof(struct lexer *lexer) { return lexer->pos >= sdslen(lexer->src); }

int lexer_peek(struct lexer *lexer) {
  if (lexer_eof(lexer)) {
    return EOF;
  }
  return lexer->src[lexer->pos];
}

void lexer_eat(struct lexer *lexer, char c) {
  if (lexer_peek(lexer) == EOF) {
    compiler_error(lexer, "Unexpected EOF, expected '%c'", c);
  }
  if (c == '\0' || lexer_peek(lexer) == c) {
    if (lexer_peek(lexer) == '\n') {
      lexer->ln++;
    }
    lexer->pos++;
    return;
  }
  compiler_error(lexer, "Expected '%c', but got '%c'", c,
                 lexer->src[lexer->pos]);
}

#define lexer_consume(lexer) lexer_eat(lexer, '\0')

bool lexer_try_eat_str(struct lexer *lexer, char *str) {
  struct lexer snapshot;
  lexer_snapshot(&snapshot, lexer);
  while (lexer_peek(lexer) == *str) {
    lexer_eat(lexer, *str);
    str++;
  }
  if (strlen(str) == 0) {
    return true;
  }
  // restore the snapshot
  lexer_snapshot(lexer, &snapshot);
  return false;
}

void lexer_skip_whitespace(struct lexer *lexer) {
  while (true) {
    if (lexer_eof(lexer)) {
      return;
    }
    while (isspace(lexer_peek(lexer))) {
      lexer_consume(lexer);
    }
    if (lexer_try_eat_str(lexer, "//")) {
      while (!lexer_eof(lexer) && lexer_peek(lexer) != '\n') {
        lexer_consume(lexer);
      }
    } else if (lexer_try_eat_str(lexer, "/*")) {
      while (!lexer_eof(lexer) && !lexer_try_eat_str(lexer, "*/")) {
        lexer_consume(lexer);
      }
    } else {
      break;
    }
  }
}

struct token_table_entry token_kw_table[] = {
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

struct token_table_entry token_multi_char_sym_table[] = {
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
lexer_get_token_type_in_token_table(char *str,
                                    struct token_table_entry *table) {

  while (table->str) {
    if (strcmp(str, table->str) == 0) {
      return table->type;
    }
    table++;
  }
  return TOK_UNKNOWN;
}

char *lexer_get_token_str_in_token_table(enum tok_type type,
                                         struct token_table_entry *table) {
  if (table == NULL) {
    table = token_multi_char_sym_table;
    while (table->str) {
      if (type == table->type) {
        return table->str;
      }
      table++;
    }
    table = token_kw_table;
    while (table->str) {
      if (type == table->type) {
        return table->str;
      }
      table++;
    }
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

char *lexer_token_to_string(int type) {
  static char buf[16] = {0};
  if (type == TOK_EOF) {
    return "EOF";
  } else if (type < 256) {
    memset(buf, 0, sizeof(buf));
    buf[0] = type;
    return buf;
  }
  return lexer_get_token_str_in_token_table(type, NULL);
}

static inline bool lexer_is_ident_start(char c) {
  return isalpha(c) || c == '_' || c == '$';
}

static inline bool lexer_is_ident_body(char c) {
  return isalnum(c) || c == '_' || c == '$';
}
static inline bool lexer_is_oct_digit(int c) { return c >= '0' && c <= '7'; }

int lexer_next_ident(struct lexer *lexer) {
  sds ident = sdsempty();
  char c;
  while (lexer_is_ident_body(c = lexer_peek(lexer))) {
    ident = sdscatlen(ident, &c, 1);
    lexer_consume(lexer);
  }
  enum tok_type kw_type =
      lexer_get_token_type_in_token_table(ident, token_kw_table);
  if (kw_type != TOK_UNKNOWN) {
    sdsfree(ident);
    return kw_type;
  }
  lexer->lex_token._ident = ident;
  return TOK_IDENT;
}

int lexer_next_number(struct lexer *lexer, bool decimal_only) {
  int base = 10;
  sds number = sdsempty();
  bool is_fp = false;
  char c;
  if (decimal_only) {
    number = sdscatlen(number, "0.", 2);
    is_fp = true;
  } else {
    // prefix
    if (lexer_peek(lexer) == '0') {
      number = sdscatlen(number, "0", 1);
      lexer_consume(lexer);
      if (tolower(lexer_peek(lexer)) == 'x') {
        number = sdscatlen(number, "x", 1);
        lexer_consume(lexer);
        base = 16;
      } else if (lexer_is_oct_digit(lexer_peek(lexer))) {
        base = 8;
      }
    }
  }

  if (base == 16) {
    while (isxdigit(c = lexer_peek(lexer))) {
      number = sdscatlen(number, &c, 1);
      lexer_consume(lexer);
    }
  } else if (base == 8) {
    while (lexer_is_oct_digit(c = lexer_peek(lexer))) {
      number = sdscatlen(number, &c, 1);
      lexer_consume(lexer);
    }
  } else {
    while (isdigit(c = lexer_peek(lexer))) {
      number = sdscatlen(number, &c, 1);
      lexer_consume(lexer);
    }
    c = lexer_peek(lexer);
    if (c == '.' || tolower(c) == 'e') {
      if (lexer_peek(lexer) == '.') {
        number = sdscatlen(number, ".", 1);
        lexer_consume(lexer);
        is_fp = true;
      } else {
        number = sdscatlen(number, "e", 1);
        lexer_consume(lexer);
        c = lexer_peek(lexer);
        if (c == '+' || c == '-') {
          // 1e+1 1e-1
          number = sdscatlen(number, &c, 1);
          lexer_consume(lexer);
        }
      }
    }
    while (isdigit(c = lexer_peek(lexer))) {
      number = sdscatlen(number, &c, 1);
      lexer_consume(lexer);
    }
  }
  bool has_u, has_l, has_f;
  has_u = has_l = has_f = false;
  while (strchr("ulf", tolower(c = lexer_peek(lexer)))) {
    switch (tolower(c)) {
    case 'u':
      if (has_u) {
        compiler_error(lexer, "Repeated number suffix: %c", c);
      }
      has_u = true;
      break;
    case 'l':
      if (has_l) {
        compiler_error(lexer, "Repeated number suffix: %c", c);
      }
      has_l = true;
      break;
    case 'f':
      if (has_f) {
        compiler_error(lexer, "Repeated number suffix: %c", c);
      }
      is_fp = has_f = true;
      break;
    }
    lexer_consume(lexer);
  }

  if (is_fp && has_f) {
    lexer->lex_token._float = strtof(number, NULL);
  } else if (is_fp && !has_f) {
    lexer->lex_token._double = strtod(number, NULL);
  } else if (has_u) {
    lexer->lex_token._uint = strtoul(number, NULL, base);
  } else if (has_l) {
    lexer->lex_token._int = strtol(number, NULL, base);
  } else {
    lexer->lex_token._int = strtol(number, NULL, base);
  }
  sdsfree(number);
  if (is_fp) {
    if (has_l) {
      compiler_error(lexer, "Invalid number suffix: l");
    }
    if (has_u) {
      compiler_error(lexer, "Invalid number suffix: u");
    }
    if (has_f) {
      return TOK_LIT_FLOAT;
    }
    return TOK_LIT_DOUBLE;
  }
  if (has_l && has_u) {
    return TOK_LIT_ULONG;
  } else if (has_l) {
    return TOK_LIT_LONG;
  } else if (has_u) {
    if (lexer->lex_token._uint > LONG_MAX) {
      return TOK_LIT_ULONG;
    }
    return TOK_LIT_UINT;
  }
  if (lexer->lex_token._int > INT_MAX || lexer->lex_token._int < INT_MIN) {
    return TOK_LIT_LONG;
  }
  return TOK_LIT_INT;
}

int lexer_next_char(struct lexer *lexer) {
  lexer_eat(lexer, '\'');
  int c;
  size_t idx = 0;
  char buf[64];
  while ((c = lexer_peek(lexer)) != '\'') {
    if (c == '\\') {
      // hack method
      char *endptr = lexer->src;
      buf[idx] = convert_decode_char(lexer->src + lexer->pos + 1, &endptr);
      // update the lexer position
      lexer->pos = endptr - lexer->src - 1;
    } else {
      buf[idx] = c;
    }
    idx++;
    lexer_consume(lexer);
  }
  lexer_eat(lexer, '\'');

  buf[idx] = '\0';
  if (idx == 0) {
    compiler_error(lexer, "Empty character constant");
  } else if (buf[0] == '\\') {
    buf[0] = convert_decode_char(buf + 1, NULL);
  } else if (idx > 1) {
    compiler_error(lexer, "Multi-character constant starts without '\\':%s",
                   buf);
  }
  lexer->lex_token._char = buf[0];
  return TOK_LIT_CHAR;
}

int lexer_next_string(struct lexer *lexer) {
  lexer_eat(lexer, '"');
  sds str = sdsempty();
  char c;
  while ((c = lexer_peek(lexer)) != '"') {
    if (c == '\\') {
      // hack method
      char *endptr = lexer->src;
      c = convert_decode_char(lexer->src + lexer->pos + 1, &endptr);
      // update the lexer position
      lexer->pos = endptr - lexer->src - 1;
    }
    str = sdscatlen(str, &c, 1);
    lexer_consume(lexer);
  }
  lexer_eat(lexer, '"');
  lexer->lex_token._str = str;
  return TOK_LIT_STRING;
}

int lexer_next_token(struct lexer *lexer) {
  lexer_skip_whitespace(lexer);
  if (lexer_eof(lexer)) {
    return TOK_EOF;
  }
  char c = lexer_peek(lexer);
  if (lexer_is_ident_start(c)) {
    return lexer_next_ident(lexer);
  } else if (isdigit(c)) {
    return lexer_next_number(lexer, false);
  }
  // multi-char symbol
  struct token_table_entry *table = token_multi_char_sym_table;
  while (table->str) {
    if (lexer_try_eat_str(lexer, table->str)) {
      return table->type;
    }
    table++;
  }

  if (c == '.') {
    lexer_consume(lexer);
    if (isdigit(lexer_peek(lexer))) {
      return lexer_next_number(lexer, true);
    }
    return '.';
  } else if (c == '\'') {
    return lexer_next_char(lexer);
  } else if (c == '"') {
    return lexer_next_string(lexer);
  }
  // single char
  lexer_consume(lexer);
  return c;
}

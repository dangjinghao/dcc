#include "ast.h"
#include "lexer.h"
#include "sds/sds.h"
#include <assert.h>
#include <stdlib.h>
/**
 * @brief input the decoded char part,
 * e.g. convert '\n' -> input 'n' -> return 0x10, \777 -> 777 -> 0x1ff
 * 
 * @param c 
 * @return char 
 */
char convert_decode_char(char *c, char **endptr) {
  if (endptr)
    *endptr = c + 1;
  switch (c[0]) {
  case 'n':
    return '\n';

  case '\\':
    return '\\';

  case 't':
    return '\t';

  case 'a':
    return '\a';

  case 'b':
    return '\b';

  case 'f':
    return '\f';

  case 'r':
    return '\r';

  case 'v':
    return '\v';

  case '\'':
    return '\'';

  case '\"':
    return '\"';

  case '?':
    return '\?';

  case 'x':
    return (char)strtol(c + 1, endptr, 16);

  default:
    // in fact, \0 is octal
    // octal
    return (char)strtol(c, endptr, 8);
  }
  return 0;
}

sds convert_expr_obj_to_repr(astn n, sds buf) {
  if (!n) {
    return buf;
  }
  buf = sdscatlen(buf, "(", 1);

  switch (n->type) {
  case ast_expr_primary:
    switch (n->value.type) {
    case TOK_LIT_INT:
      buf = sdscatprintf(buf, "%ldi", n->value.v._int);
      break;
    case TOK_LIT_UINT:
      buf = sdscatprintf(buf, "%luu", n->value.v._uint);
      break;
    case TOK_LIT_LONG:
      buf = sdscatprintf(buf, "%ldl", n->value.v._int);
      break;
    case TOK_LIT_ULONG:
      buf = sdscatprintf(buf, "%luul", n->value.v._uint);
      break;
    case TOK_LIT_FLOAT:
      buf = sdscatprintf(buf, "%f", n->value.v._float);
      break;
    case TOK_LIT_DOUBLE:
      buf = sdscatprintf(buf, "%f", n->value.v._double);
      break;
    case TOK_LIT_CHAR:
      buf = sdscatlen(buf, "'", 1);
      buf = sdscatrepr(buf, &n->value.v._char, 1);
      buf = sdscatlen(buf, "'", 1);
      break;
    case TOK_LIT_STRING:
      buf = sdscatlen(buf, "\"", 1);
      buf = sdscatrepr(buf, n->value.v._str, sdslen(n->value.v._str));
      buf = sdscatlen(buf, "\"", 1);
      break;
    case TOK_IDENT:
      buf = sdscatlen(buf, n->value.v._ident, sdslen(n->value.v._ident));
      break;
    default:
      break;
    }
    break;
  case ast_expr_unary:
    if (!n->unary.postfix) {
      switch (n->unary.op) {
      case TOK_SYM_SELF_INC:
        buf = sdscatlen(buf, "++", 2);
        break;
      case TOK_SYM_SELF_DEC:
        buf = sdscatlen(buf, "--", 2);
        break;
      case TOK_SYM_ARROW:
        buf = sdscatlen(buf, "->", 2);
        break;
      case TOK_KW_SIZEOF:
        buf = sdscatlen(buf, "sizeof ", 7);
        break;
      case '-':
        buf = sdscatlen(buf, "-", 1);
        break;
      case '+':
        buf = sdscatlen(buf, "+", 1);
        break;
      case '!':
        buf = sdscatlen(buf, "!", 1);
        break;
      case '~':
        buf = sdscatlen(buf, "~", 1);
        break;
      case '*':
        buf = sdscatlen(buf, "*", 1);
        break;
      case '&':
        buf = sdscatlen(buf, "&", 1);
        break;
      default:
        assert(0 && "this unary postfix op is not supported");
      }
    }
    buf = convert_expr_obj_to_repr(n->unary.expr, buf);
    if (n->unary.postfix) {
      switch (n->unary.op) {
      case TOK_SYM_SELF_INC:
        buf = sdscatlen(buf, "++", 2);
        break;
      case TOK_SYM_SELF_DEC:
        buf = sdscatlen(buf, "--", 2);
        break;
      default:
        assert(0 && "this unary postfix op is not supported");
      }
    }
    break;
  case ast_expr_binop:
    buf = convert_expr_obj_to_repr(n->binop.lhs, buf);
    char op = n->binop.op;
    buf = sdscatlen(buf, &op, 1);
    buf = convert_expr_obj_to_repr(n->binop.rhs, buf);
    break;
  case ast_expr_ternary:
    break;
  }
  buf = sdscatlen(buf, ")", 1);
  return buf;
}
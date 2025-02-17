#include "ast.h"
#include "convert.h"
#include "dynarray/dynarray.h"
#include "lexer.h"
#include "log/log.h"
#include "macro/macro.h"
#include "sds/sds.h"
#include <assert.h>
#include <stddef.h>
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

void __convert_print_ast_repr(astn n) {
  sds buf = sdsempty();
  buf = convert_ast_to_repr(n, buf);
  printf("%s\n", buf);
  sdsfree(buf);
}

sds convert_enum_qualifier_to_string(enum type_qualifier q, sds buf) {
  if (q == TYPE_QUAL_NONE) {
    return sdscat(buf, "NONE");
  }
  int c = 0;
  if (q & TYPE_QUAL_CONST) {
    buf = sdscat(buf, "CONST");
  }
  if (q & TYPE_QUAL_VOLATILE) {
    if (c) {
      buf = sdscat(buf, " | ");
    }
    buf = sdscat(buf, "VOLATILE");
    c++;
  }
  if (q & TYPE_QUAL_RESTRICT) {
    if (c) {
      buf = sdscat(buf, " | ");
    }
    buf = sdscat(buf, "RESTRICT");
    c++;
  }
  if (q & TYPE_QUAL_INLINE) {
    if (c) {
      buf = sdscat(buf, " | ");
    }
    buf = sdscat(buf, "INLINE");
    c++;
  }
  return buf;
}

sds convert_ast_to_repr(astn n, sds buf) {
  if (!n) {
    return buf;
  }
  buf = sdscatlen(buf, "(", 1);

  switch (n->type) {
  case ast_expr_primary:
    switch (n->primary.type) {
    case TOK_LIT_INT:
      buf = sdscatprintf(buf, "%ldi", n->primary.v._int);
      break;
    case TOK_LIT_UINT:
      buf = sdscatprintf(buf, "%luu", n->primary.v._uint);
      break;
    case TOK_LIT_LONG:
      buf = sdscatprintf(buf, "%ldl", n->primary.v._int);
      break;
    case TOK_LIT_ULONG:
      buf = sdscatprintf(buf, "%luul", n->primary.v._uint);
      break;
    case TOK_LIT_FLOAT:
      buf = sdscatprintf(buf, "%f", n->primary.v._float);
      break;
    case TOK_LIT_DOUBLE:
      buf = sdscatprintf(buf, "%f", n->primary.v._double);
      break;
    case TOK_LIT_CHAR:
      buf = sdscatrepr(buf, &n->primary.v._char, 1);
      break;
    case TOK_LIT_STRING:
      buf = sdscatrepr(buf, n->primary.v._str, sdslen(n->primary.v._str));
      break;
    default:
      assert(0 && "this primary type is not supported");
    }
    break;
  case ast_ident:
    buf = sdscatsds(buf, n->ident);
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
      default: {
        char c = n->unary.op;
        buf = sdscatlen(buf, &c, 1);
      }
      }
    }
    buf = convert_ast_to_repr(n->unary.expr, buf);
    if (n->unary.postfix) {
      switch (n->unary.op) {
      case TOK_SYM_SELF_INC:
        buf = sdscatlen(buf, "++", 2);
        break;
      case TOK_SYM_SELF_DEC:
        buf = sdscatlen(buf, "--", 2);
        break;
      default:
        assert(0 && "this unary postfix op is not supported yet");
      }
    }
    break;
  case ast_expr_binop:
    buf = convert_ast_to_repr(n->binop.lhs, buf);
    char op = n->binop.op;
    buf = sdscatlen(buf, &op, 1);
    buf = convert_ast_to_repr(n->binop.rhs, buf);
    break;
  case ast_expr_ternary:
    buf = convert_ast_to_repr(n->ternary.cond, buf);
    buf = sdscatlen(buf, "?", 1);
    buf = convert_ast_to_repr(n->ternary._t, buf);
    buf = sdscatlen(buf, ":", 1);
    buf = convert_ast_to_repr(n->ternary._f, buf);
    break;
  case ast_declaration: {
    buf = sdscatlen(buf, "declaration[", 12);
    if (n->declaration.ident) {
      buf = sdscatsds(buf, n->declaration.ident);
    }
    buf = sdscatlen(buf, " ", 1);
    astn *ref;
    dynarray_foreach(n->declaration.type_chain, ref) {
      buf = convert_ast_to_repr(*ref, buf);
      buf = sdscatlen(buf, " ", 1);
    }
    if (n->declaration.extdata) {
      buf = sdscatlen(buf, "= ", 2);
      buf = convert_ast_to_repr(n->declaration.extdata, buf);
    }
    buf = sdscatlen(buf, "]", 1);
    break;
  }
  case ast_block: {
    buf = sdscat(buf, "block[stmts[");
    astn *ref;
    dynarray_foreach(n->block.stmts, ref) {
      buf = convert_ast_to_repr(*ref, buf);
      buf = sdscatlen(buf, " ", 1);
    }
    buf = sdscatlen(buf, "]]", 2);
    break;
  }
  case ast_ctype:
    buf = sdscatlen(buf, "ctype[", 6);

    if (n->ctype.qualifier != TYPE_QUAL_NONE) {
      buf = convert_enum_qualifier_to_string(n->ctype.qualifier, buf);
      buf = sdscatlen(buf, " ", 1);
    }
    if (n->ctype.storage != TOK_UNKNOWN) {
      buf = sdscatprintf(buf, "%s ",
                         convert_token_type_to_string(n->ctype.storage));
    }
    if (n->ctype.signint != TOK_UNKNOWN) {
      buf = sdscatprintf(buf, "%s ",
                         convert_token_type_to_string(n->ctype.signint));
    }
    if (n->ctype.type != TOK_UNKNOWN) {
      buf =
          sdscatprintf(buf, "%s ", convert_token_type_to_string(n->ctype.type));
    }
    if (n->ctype.user_defined_type) {
      buf = sdscatlen(buf, "user_defined_type[", 17);
      buf = convert_ast_to_repr(n->ctype.user_defined_type, buf);
      buf = sdscatlen(buf, "]", 1);
    }
    buf = sdscatlen(buf, "]", 1);
    break;
  case ast_labeled_statement:
    buf = sdscatprintf(buf, "%s ",
                       convert_token_type_to_string(n->labeled_statement.type));
    if (n->labeled_statement.label_value) {
      buf = convert_ast_to_repr(n->labeled_statement.label_value, buf);
    }
    buf = convert_ast_to_repr(n->labeled_statement.stmt, buf);

    break;
  }
  buf = sdscatlen(buf, ")", 1);
  return buf;
}

enum type_qualifier convert_token_type_to_qualifier(enum tok_type tok) {
  switch (tok) {
  case TOK_KW_CONST:
    return TYPE_QUAL_CONST;
  case TOK_KW_VOLATILE:
    return TYPE_QUAL_VOLATILE;
  // case TOK_KW_RESTRICT:
  //   return TYPE_QUAL_RESTRICT;
  // case TOK_KW_INLINE:
  //   return TYPE_QUAL_INLINE;
  default:
    log_panic("invalid type qualifier token");
    return TYPE_QUAL_NONE;
  }
}

char *convert_token_type_to_string(enum tok_type tok) {
  static char b;
  switch (tok) {
    STRCASE(TOK_UNKNOWN);
    STRCASE(TOK_IDENT);
    STRCASE(TOK_KW_INT);
    STRCASE(TOK_KW_FLOAT);
    STRCASE(TOK_KW_VOID);
    STRCASE(TOK_KW_CHAR);
    STRCASE(TOK_KW_ENUM);
    STRCASE(TOK_KW_SIZEOF);
    STRCASE(TOK_KW_AUTO);
    STRCASE(TOK_KW_CASE);
    STRCASE(TOK_KW_CONST);
    STRCASE(TOK_KW_CONTINUE);
    STRCASE(TOK_KW_DEFAULT);
    STRCASE(TOK_KW_DO);
    STRCASE(TOK_KW_DOUBLE);
    STRCASE(TOK_KW_ELSE);
    STRCASE(TOK_KW_EXTERN);
    STRCASE(TOK_KW_FOR);
    STRCASE(TOK_KW_GOTO);
    STRCASE(TOK_KW_IF);
    STRCASE(TOK_KW_LONG);
    STRCASE(TOK_KW_REGISTER);
    STRCASE(TOK_KW_RETURN);
    STRCASE(TOK_KW_SHORT);
    STRCASE(TOK_KW_SIGNED);
    STRCASE(TOK_KW_STATIC);
    STRCASE(TOK_KW_STRUCT);
    STRCASE(TOK_KW_SWITCH);
    STRCASE(TOK_KW_TYPEDEF);
    STRCASE(TOK_KW_UNION);
    STRCASE(TOK_KW_UNSIGNED);
    STRCASE(TOK_KW_VOLATILE);
    STRCASE(TOK_KW_WHILE);
    STRCASE(TOK_SYM_LEQ);
    STRCASE(TOK_SYM_GEQ);
    STRCASE(TOK_SYM_EQ);
    STRCASE(TOK_SYM_NEQ);
    STRCASE(TOK_SYM_SELF_ADD);
    STRCASE(TOK_SYM_SELF_SUB);
    STRCASE(TOK_SYM_SELF_MUL);
    STRCASE(TOK_SYM_SELF_DIV);
    STRCASE(TOK_SYM_SELF_MOD);
    STRCASE(TOK_SYM_SELF_BIT_OR);
    STRCASE(TOK_SYM_SELF_BIT_XOR);
    STRCASE(TOK_SYM_SELF_BIT_AND);
    STRCASE(TOK_SYM_RSHIFT);
    STRCASE(TOK_SYM_SELF_RSHIFT);
    STRCASE(TOK_SYM_LSHIFT);
    STRCASE(TOK_SYM_SELF_LSHIFT);
    STRCASE(TOK_SYM_ARROW);
    STRCASE(TOK_SYM_SELF_INC);
    STRCASE(TOK_SYM_SELF_DEC);
    STRCASE(TOK_SYM_LOGIC_OR);
    STRCASE(TOK_SYM_LOGIC_AND);
    STRCASE(TOK_KW_BREAK);
    STRCASE(TOK_KW_VARARGS);

  case TOK_EOF:
  case __TOK_KW_START:
  case __TOK_KW_END:
  case __TOK_LIT_START:
  case TOK_LIT_INT:
  case TOK_LIT_UINT:
  case TOK_LIT_LONG:
  case TOK_LIT_ULONG:
  case TOK_LIT_FLOAT:
  case TOK_LIT_DOUBLE:
  case TOK_LIT_CHAR:
  case TOK_LIT_STRING:
  case __TOK_LIT_END:
    log_panic("invalid token type: %d", tok);
    break;
  }
  b = tok;
  return &b;
}

size_t convert_token_type_to_size(enum tok_type t) {
  switch (t) {
  case TOK_KW_INT:
    return sizeof(int);
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
  case TOK_KW_VOID:
    return sizeof(void);
  default:
    log_panic("unsupported type:`%s`", convert_token_type_to_string(t));
  }
  return 0;
}

char *convert_ast_type_to_string(enum ast_type t) {
  switch (t) {
    STRCASE(ast_declaration);
    STRCASE(ast_expr_binop);
    STRCASE(ast_expr_primary);
    STRCASE(ast_expr_ternary);
    STRCASE(ast_expr_unary);
    STRCASE(ast_ident);
    STRCASE(ast_block);
    STRCASE(ast_ctype);
    STRCASE(ast_labeled_statement);
  }
  return NULL;
}

char *convert_type_qualifier(enum type_qualifier t) {
  switch (t) {
    STRCASE(TYPE_QUAL_NONE);
    STRCASE(TYPE_QUAL_CONST);
    STRCASE(TYPE_QUAL_VOLATILE);
    STRCASE(TYPE_QUAL_RESTRICT);
    STRCASE(TYPE_QUAL_INLINE);
  }
  return NULL;
}
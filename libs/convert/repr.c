#include "ast.h"
#include "convert.h"
#include "lexer.h"
#include "log/log.h"
#include "macro/macro.h"
#include "sds/sds.h"
#include <assert.h>
#include <stdbool.h>
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
  buf = convert_ast_to_json(n, buf, false);
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

/**
 * @brief 
 * 
 * @param n 
 * @param buf 
 * @param shallow true: print the ast node only, not the children
 * @return sds 
 */
sds convert_ast_to_json(astn n, sds buf, bool shallow) {
  if (!n) {
    return buf;
  }

  switch (n->type) {
  case ast_expr_primary:
    switch (n->primary.type) {
    case TOK_LIT_INT:
      buf =
          sdscatprintf(buf, "{\"name\":\"<%s-int> %ld\"}",
                       convert_ast_type_to_string(n->type), n->primary.v._int);
      break;
    case TOK_LIT_UINT:
      buf =
          sdscatprintf(buf, "{\"name\":\"<%s-uint> %lu\"}",
                       convert_ast_type_to_string(n->type), n->primary.v._uint);
      break;
    case TOK_LIT_LONG:
      buf =
          sdscatprintf(buf, "{\"name\":\"<%s-long> %ld\"}",
                       convert_ast_type_to_string(n->type), n->primary.v._int);
      break;
    case TOK_LIT_ULONG:
      buf =
          sdscatprintf(buf, "{\"name\":\"<%s-ulong> %lu\"}",
                       convert_ast_type_to_string(n->type), n->primary.v._uint);
      break;
    case TOK_LIT_FLOAT:
      buf = sdscatprintf(buf, "{\"name\":\"<%s-float> %f\"}",
                         convert_ast_type_to_string(n->type),
                         n->primary.v._float);
      break;
    case TOK_LIT_DOUBLE:
      buf = sdscatprintf(buf, "{\"name\":\"<%s-double> %f\"}",
                         convert_ast_type_to_string(n->type),
                         n->primary.v._double);
      break;
    case TOK_LIT_CHAR:
      buf =
          sdscatprintf(buf, "{\"name\":\"<%s-char> %c\"}",
                       convert_ast_type_to_string(n->type), n->primary.v._char);
      break;
    case TOK_LIT_STRING:
      buf =
          sdscatprintf(buf, "{\"name\":\"<%s-string> %s\"}",
                       convert_ast_type_to_string(n->type), n->primary.v._str);
      break;
    default:
      log_panic("this primary type is not supported");
    }
    break;
  case ast_ident:
    buf = sdscatprintf(buf, "{\"name\":\"id-%s\"}", n->ident);
    break;
  case ast_expr_unary: {
    buf = sdscatprintf(buf, "{\"name\":\"<%s-%s> %s\",\"children\":[",
                       convert_ast_type_to_string(n->type),
                       n->unary.postfix ? "post" : "front",
                       convert_token_type_to_string(n->unary.op));
    if (!shallow) {
      buf = convert_ast_to_json(n->unary.expr, buf, shallow);
    } else {
      buf = sdscat(buf, "{\"name\":\"<...>\"}");
    }
    buf = sdscat(buf, ",");
    if (n->unary.extdata) {
      buf = convert_ast_to_json(n->unary.extdata, buf, shallow);
    }
    if (buf[sdslen(buf) - 1] == ',') {
      sdssetlen(buf, sdslen(buf) - 1);
    }
    buf = sdscat(buf, "]}");
    break;
  }
  case ast_expr_binop: {
    buf = sdscatprintf(buf, "{\"name\":\"%s\",\"children\":[",
                       convert_token_type_to_string(n->binop.op));
    if (!shallow) {
      buf = convert_ast_to_json(n->binop.lhs, buf, shallow);
      buf = sdscat(buf, ",");
      buf = convert_ast_to_json(n->binop.rhs, buf, shallow);
    } else {
      buf = sdscat(buf, "{\"name\":\"<...>\"}");
    }
    buf = sdscat(buf, "]}");
    break;
  }
  case ast_expr_ternary: {
    buf = sdscatprintf(buf, "{\"name\":\"%s\",\"children\":[",
                       convert_ast_type_to_string(n->type));
    if (!shallow) {
      buf = convert_ast_to_json(n->ternary.cond, buf, shallow);
      buf = sdscat(buf, ",");
      buf = convert_ast_to_json(n->ternary._t, buf, shallow);
      buf = sdscat(buf, ",");
      buf = convert_ast_to_json(n->ternary._f, buf, shallow);
      buf = sdscat(buf, "]}");
    } else {
      buf = sdscat(buf, "{\"name\":\"<...>\"}");
    }
    break;
  }
  case ast_declaration: {
    buf = sdscatprintf(buf, "{\"name\":\"%s\",\"children\":[",
                       convert_ast_type_to_string(n->type));
    if (n->declaration.ident) {
      buf = sdscatprintf(buf, "{\"name\":\"<id> %s.%ld\"},",
                         n->declaration.ident, n->declaration.uid);
    } else {
      buf = sdscatprintf(buf, "{\"name\":\"<empty-id>\"},");
    }

    {
      buf =
          sdscatprintf(buf, "{\"name\":\"<type-declaration> \",\"children\":[");
      astn ref;
      if (!shallow) {
        slist_foreach(&n->declaration.type_chain, ref) {
          buf = convert_ast_to_json(ref, buf, shallow);
          buf = sdscat(buf, ",");
        }
      } else {
        buf = sdscat(buf, "{\"name\":\"<...>\"},");
      }
      if (buf[sdslen(buf) - 1] == ',') {
        sdssetlen(buf, sdslen(buf) - 1);
      }
      buf = sdscat(buf, "]},");
    }

    if (n->declaration.extdata && !shallow) {
      buf = convert_ast_to_json(n->declaration.extdata, buf, shallow);
    } else if (buf[sdslen(buf) - 1] == ',') {
      sdssetlen(buf, sdslen(buf) - 1);
    }

    buf = sdscat(buf, "]}");
    break;
  }
  case ast_initializer: {
    buf = convert_ast_to_json(n->initializer.init, buf, shallow);
    break;
  }
  case ast_block: {
    buf = sdscat(buf, "{\"name\":\"block\",\"children\":[");
    astn ref;
    if (!shallow) {
      slist_foreach(&n->block.list, ref) {
        buf = convert_ast_to_json(ref, buf, shallow);
        buf = sdscat(buf, ",");
      }
    } else {
      buf = sdscat(buf, "{\"name\":\"<...>\"},");
    }
    if (buf[sdslen(buf) - 1] == ',') {
      sdssetlen(buf, sdslen(buf) - 1);
    }
    buf = sdscat(buf, "]}");
    break;
  }
  case ast_parameters: {
    buf = sdscat(buf, "{\"name\":\"parameters\",\"children\":[");
    astn ref;
    if (!shallow) {
      slist_foreach(&n->parameters.list, ref) {
        buf = convert_ast_to_json(ref, buf, shallow);
        buf = sdscat(buf, ",");
      }
    } else {
      buf = sdscat(buf, "{\"name\":\"<...>\"},");
    }
    if (buf[sdslen(buf) - 1] == ',') {
      sdssetlen(buf, sdslen(buf) - 1);
    }
    buf = sdscat(buf, "]}");
    break;
  }
  case ast_arguments: {
    buf = sdscat(buf, "{\"name\":\"arguments\",\"children\":[");
    astn ref;
    if (!shallow) {
      slist_foreach(&n->arguments.list, ref) {
        buf = convert_ast_to_json(ref, buf, shallow);
        buf = sdscat(buf, ",");
      }
    } else {
      buf = sdscat(buf, "{\"name\":\"<...>\"},");
    }
    if (buf[sdslen(buf) - 1] == ',') {
      sdssetlen(buf, sdslen(buf) - 1);
    }
    buf = sdscat(buf, "]}");
    break;
  }
  case ast_trans_unit: {
    buf = sdscat(buf, "{\"name\":\"trans_unit\",\"children\":[");
    astn ref;
    if (!shallow) {
      slist_foreach(&n->trans_unit.list, ref) {
        buf = convert_ast_to_json(ref, buf, shallow);
        buf = sdscat(buf, ",");
      }
    } else {
      buf = sdscat(buf, "{\"name\":\"<...>\"},");
    }
    if (buf[sdslen(buf) - 1] == ',') {
      sdssetlen(buf, sdslen(buf) - 1);
    }
    buf = sdscat(buf, "]}");
    break;
  }
  case ast_ctype:
    buf = sdscat(buf, "{\"name\":\"ctype\",\"children\":[");
    if (n->ctype.storage != TOK_UNKNOWN) {
      buf = sdscatprintf(buf, "{\"name\":\"<storage> %s\"}",
                         convert_token_type_to_string(n->ctype.storage));
      buf = sdscat(buf, ",");
    }
    if (n->ctype.qualifier != TYPE_QUAL_NONE) {
      sds qual_buf = sdsempty();
      buf = sdscatprintf(
          buf, "{\"name\":\"<qualifier> %s\"}",
          convert_enum_qualifier_to_string(n->ctype.qualifier, qual_buf));
      sdsfree(qual_buf);
      buf = sdscat(buf, ",");
    }
    if (n->ctype.signint != TOK_UNKNOWN) {
      buf = sdscatprintf(buf, "{\"name\":\"<signint> %s\"}",
                         convert_token_type_to_string(n->ctype.signint));
      buf = sdscat(buf, ",");
    }
    if (n->ctype.type != TOK_UNKNOWN) {
      buf = sdscatprintf(buf, "{\"name\":\"%s\"}",
                         convert_token_type_to_string(n->ctype.type));
      buf = sdscat(buf, ",");
    }
    if (!shallow) {
      buf = convert_ast_to_json(n->ctype.user_defined_type, buf, shallow);
    } else {
      buf = sdscat(buf, "{\"name\":\"<...>\"}");
    }
    if (buf[sdslen(buf) - 1] == ',') {
      sdssetlen(buf, sdslen(buf) - 1);
    }
    buf = sdscat(buf, "]}");
    break;
  case ast_labeled_statement:
    buf = sdscatprintf(buf, "{\"name\":\"%s: \",\"children\":[",
                       convert_token_type_to_string(n->labeled_statement.type));

    if (n->labeled_statement.label_value && !shallow) {
      buf = convert_ast_to_json(n->labeled_statement.label_value, buf, shallow);
      buf = sdscat(buf, ",");
    }
    buf = convert_ast_to_json(n->labeled_statement.stmt, buf, shallow);
    buf = sdscat(buf, "]}");
    break;
  case ast_expr_typecast:
    buf = sdscat(buf, "{\"name\":\"typecast\",\"children\":[");
    astn ref;
    if (!shallow) {
      slist_foreach(&n->typecast.type_chain, ref) {
        buf = convert_ast_to_json(ref, buf, shallow);
        buf = sdscat(buf, ",");
      }
    } else {
      buf = sdscat(buf, "{\"name\":\"<...>\"},");
    }
    buf = convert_ast_to_json(n->typecast.expr, buf, shallow);
    buf = sdscat(buf, "]}");
    break;
  case ast_ref:
    buf = sdscatprintf(buf, "{\"name\":\"ref\",\"children\":[");
    buf = convert_ast_to_json(n->ref, buf, true);
    buf = sdscat(buf, "]}");
    break;
  case ast_struct_union_declaration:
    buf = sdscatprintf(buf, "{\"name\":\"struct/union\",\"children\":[");
    if (n->struct_union_declaration.ident) {
      buf = sdscatprintf(buf, "{\"name\":\"<id> %s\"},",
                         n->struct_union_declaration.ident);
    } else {
      buf = sdscatprintf(buf, "{\"name\":\"<empty-id>\"},");
    }
    if (!shallow) {
      astn ref;
      slist_foreach(&n->struct_union_declaration.member_declarations, ref) {
        buf = convert_ast_to_json(ref, buf, shallow);
        buf = sdscat(buf, ",");
      }
    } else {
      buf = sdscat(buf, "{\"name\":\"<...>\"}");
    }
    if (buf[sdslen(buf) - 1] == ',') {
      sdssetlen(buf, sdslen(buf) - 1);
    }
    buf = sdscat(buf, "]}");
    break;
  case ast_initializer_list: {
    buf = sdscat(buf, "{\"name\":\"initializer_list\",\"children\":[");
    astn ref;
    if (!shallow) {
      slist_foreach(&n->initializer_list.list, ref) {
        buf = convert_ast_to_json(ref, buf, shallow);
        buf = sdscat(buf, ",");
      }
    } else {
      buf = sdscat(buf, "{\"name\":\"<...>\"},");
    }
    if (buf[sdslen(buf) - 1] == ',') {
      sdssetlen(buf, sdslen(buf) - 1);
    }
    buf = sdscat(buf, "]}");

    break;
  }
  }
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
  static char b[2] = {0};
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
    STRCASE(TOK_SYM_VARARGS);

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
  b[0] = tok;
  return b;
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
    STRCASE(ast_ctype);
    STRCASE(ast_labeled_statement);
    STRCASE(ast_expr_typecast);
    STRCASE(ast_struct_union_declaration);
    STRCASE(ast_ref);
    STRCASE(ast_parameters);
    STRCASE(ast_arguments);
    STRCASE(ast_block);
    STRCASE(ast_trans_unit);
    STRCASE(ast_initializer);
    STRCASE(ast_initializer_list);
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
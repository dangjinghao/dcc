#include "parser.h"
#include "ast.h"
#include "convert/convert.h"
#include "lexer.h"
#include "log/log.h"
#include "macro/macro.h"
#include "sds/sds.h"
#include "slist/slist.h"
#include "token.h"
#include <stdbool.h>
int parser_consume(parser parser) {
  return parser->current_token = lexer_get_next_token(parser->lexer);
}

void parser_new_from_lexer(parser parser, struct lexer *lexer) {
  parser->lexer = calloc(1, sizeof(struct lexer));
  lexer_snapshot_new(parser->lexer, lexer);
  parser_consume(parser);
  slist_init(&parser->idtab);
  slist_init(&parser->tagtab);
  slist_init(&parser->symtab);
  parser->continue_scope = parser->switch_scope = parser->break_scope =
      parser->function_scope = NULL;
  parser->uidcnt = 0;
}

void parser_new_snapshot(parser _new, parser _old) {
  *_new = *_old;
  _new->lexer = calloc(1, sizeof(struct lexer));
  lexer_snapshot_new(_new->lexer, _old->lexer);
  // bad hack
  if (_old->current_token == TOK_IDENT ||
      _old->current_token == TOK_LIT_STRING) {
    _new->lexer->lex_token._ident = sdsdup(_old->lexer->lex_token._ident);
  }
  slist_copy(&_new->idtab, &_old->idtab);
  slist_copy(&_new->tagtab, &_old->tagtab);
  slist_copy(&_new->symtab, &_old->symtab);
}

/**
 * @brief destory the target parser and move the snapshot to the target,
 * so do not free the snapshot after this function
 * 
 * @param target 
 * @param snapshot 
 */
void parser_restore(parser target, parser snapshot) {
  parser_destory(target);
  *target = *snapshot;
}

void parser_destory(parser parser) {
  // bad hack
  if (parser->current_token == TOK_IDENT ||
      parser->current_token == TOK_LIT_STRING) {
    sdsfree(parser->lexer->lex_token._ident);
  }
  slist_free(&parser->idtab);
  slist_free(&parser->tagtab);
  slist_free(&parser->symtab);
  free(parser->lexer);
}

int parser_consume_with(parser parser, int token) {
  if (parser->current_token == token) {
    return parser_consume(parser);
  }
  compiler_error(parser->lexer, "Expected token %s, got %s",
                 lexer_token_get_str(token),
                 lexer_token_get_str(parser->current_token));
  return 0;
}

static struct astn __parser_scope_fence,
    *__parser_scope_fence_ptr = &__parser_scope_fence;
void parser_push_scope(parser parser) {
  log_trace("push scope at line %ld", parser->lexer->ln);
  slist_add_head(&parser->idtab, __parser_scope_fence_ptr);
  slist_add_head(&parser->tagtab, __parser_scope_fence_ptr);
}

void parser_pop_scope(parser parser) {
  log_debug("pop scope at line %ld", parser->lexer->ln);
  astn r;
  while ((r = slist_pop_head(&parser->idtab)) != __parser_scope_fence_ptr) {
    log_debug("pop declaration %s", r->declaration.ident);
  }
}

static inline astn parser_astn_ident_cmp(astn data, sds ident) {
  if (0 == sdscmp(parse_declaration_get_ident(data), ident)) {
    return data;
  }
  return NULL;
}

astn parser_scope_all_find_ident(sds ident, slist tab) {
  astn data;
  slist_foreach(tab, data) {
    if (data == __parser_scope_fence_ptr) {
      continue;
    }
    astn r = parser_astn_ident_cmp(data, ident);
    if (r) {
      return r;
    }
  }
  return NULL;
}

astn parser_scope_current_find_ident(sds ident, slist tab) {
  astn data;
  slist_foreach(tab, data) {
    if (data == __parser_scope_fence_ptr) {
      break;
    }
    astn r = parser_astn_ident_cmp(data, ident);
    if (r) {
      return r;
    }
  }
  return NULL;
}

void parser_scope_current_add_symbol(astn n, slist tab) {
  log_debug("add declaration %s to current scope",
            parse_declaration_get_ident(n));
  slist_add_head(tab, n);
}

astn parser_lookup_typedef(parser parser, sds ident) {
  astn d = parser_scope_all_find_ident(ident, &parser->idtab);
  if (d && d->declaration.storage_class == TOK_KW_TYPEDEF) {
    return d;
  }
  return NULL;
}

bool parser_scope_is_current_global(parser parser) {
  // find NULL in the idtab
  return parser->function_scope == NULL;
}

/**
 * @brief check whether the same name declaration existing, 
 * then add declaration to the current scope table,
 * set the uid for the declaration
 * 
 * @param parser 
 * @param n 
 */
void parser_new_declaration(parser parser, astn n) {
  assert(n->type == ast_declaration);
  if (n->declaration.ident == NULL) {
    log_debug("this is an abstract declarator, skipping declaration");
    return;
  }
  astn existing_symbol =
      parser_scope_current_find_ident(n->declaration.ident, &parser->idtab);
  if (existing_symbol && n->declaration.storage_class == TOK_KW_EXTERN) {
    log_debug("multiple extern declaration, do nothing: %s",
              n->declaration.ident);
    return;
  } else if (existing_symbol && n->declaration.storage_class != TOK_KW_EXTERN &&
             existing_symbol->declaration.storage_class != TOK_KW_EXTERN) {
    compiler_error(parser->lexer, "redefined symbol %s", n->declaration.ident);
  }

  n->declaration.uid = parser_get_uid(parser);
  parser_scope_current_add_symbol(n, &parser->idtab);
  if (parser_scope_is_current_global(parser) ||
      n->declaration.storage_class == TOK_KW_EXTERN) {
    // we need add the extern symbol which is defined in block scope to symtab
    parser_symtab_add(parser, n);
  }
}
/**
 * @brief add the declarator, e.g. enum {a, b, c}; add a, b, c to the current scope
 * 
 * @param parser 
 * @param enumerator 
 */
void parser_new_enumerator(parser parser, astn enumerator) {
  assert(enumerator->type == ast_enumerator);
  if (enumerator->enumerator.ident == NULL) {
    log_debug("this is an abstract enumerator, skipping");
    return;
  }
  astn existing_sym = parser_scope_current_find_ident(
      enumerator->enumerator.ident, &parser->idtab);
  if (existing_sym) {
    compiler_error(parser->lexer, "redefined enumerator %s",
                   enumerator->enumerator.ident);
  }
  enumerator->enumerator.uid = parser_get_uid(parser);
  parser_scope_current_add_symbol(enumerator, &parser->idtab);
}
/**
 * @brief declare a new struct/union/enum tag
 * 
 * @param parser 
 * @param n 
 */
void parser_new_tag(parser parser, astn n) {
  if (n->type == ast_struct_or_union_declaration) {
    if (n->struct_or_union_declaration.ident == NULL) {
      log_debug("this is an abstract struct declarator, skipping declaration");
      return;
    }
    astn existing_tag = parser_scope_current_find_ident(
        n->struct_or_union_declaration.ident, &parser->tagtab);
    if (existing_tag) {
      compiler_error(parser->lexer, "redefined  tag with identifier %s",
                     n->struct_or_union_declaration.ident);
    }
    n->struct_or_union_declaration.uid = parser_get_uid(parser);
  } else {
    assert(n->type == ast_enumeration);
    if (n->enumeration.ident == NULL) {
      log_debug("this is an abstract enumeration, skipping declaration");
      return;
    }
    astn existing_tag =
        parser_scope_current_find_ident(n->enumeration.ident, &parser->tagtab);
    if (existing_tag) {
      compiler_error(parser->lexer, "redefined tag with identifier %s",
                     n->enumeration.ident);
    }
    n->enumeration.uid = parser_get_uid(parser);
  }

  parser_scope_current_add_symbol(n, &parser->tagtab);
}

size_t parser_get_uid(parser parser) {
  log_trace("allocating uid %ld", parser->uidcnt);
  return parser->uidcnt++;
}

void parser_unfold_type_chain(parser parser, slist type_chain) {
  astn tail = slist_peek_tail(type_chain);
  assert(tail->type == ast_ctype);
  assert(tail->ctype.storage == TOK_UNKNOWN);
  enum type_qualifier qual = tail->ctype.qualifier;
  if (tail->ctype.type != TOK_KW_TYPEDEF) {
    return;
  }
  astn n = tail->ctype.user_defined_type;
  assert(n->type == ast_ref);
  n = n->ref;
  assert(n->type == ast_declaration);
  ast_free(slist_pop_tail(type_chain));
  astn t;
  slist typedef_typechain = &n->declaration.type_chain;
  slist_foreach(typedef_typechain, t) {
    slist_add_tail(type_chain, ast_copy(t));
  }
  // reset typedef
  n = slist_peek_tail(type_chain);
  assert(n->type == ast_ctype && n->ctype.storage == TOK_UNKNOWN);
  n->ctype.qualifier |= qual;
}

long parser_expr_eval_const_int(astn expr) {
  assert(parser_expr_check_const_int(expr));
  if (expr->type == ast_ref) {
    expr = expr->ref;
  }
  switch (expr->type) {
  case ast_enumerator: {
    return expr->enumerator.value;
  }
  case ast_expr_primary: {
    switch (expr->primary.type) {
    case TOK_LIT_INT:
    case TOK_LIT_LONG:
      return expr->primary.v._int;
    case TOK_LIT_UINT:
    case TOK_LIT_ULONG:
      return expr->primary.v._uint;
    case TOK_LIT_CHAR:
      return expr->primary.v._char;
    default:
      log_panic(
          "unexpected primary type when evaluating constant int expression: %s",
          lexer_token_get_str(expr->primary.type));
    }
  }
  case ast_expr_binop: {
    long long lhs = parser_expr_eval_const_int(expr->binop.lhs);
    long long rhs = parser_expr_eval_const_int(expr->binop.rhs);
    switch (expr->binop.op) {
    case '+':
      return lhs + rhs;
    case '-':
      return lhs - rhs;
    case '*':
      return lhs * rhs;
    case '/':
      return lhs / rhs;
    case '%':
      return lhs % rhs;
    case TOK_SYM_LSHIFT:
      return lhs << rhs;
    case TOK_SYM_RSHIFT:
      return lhs >> rhs;
    case TOK_SYM_LEQ:
      return lhs <= rhs;
    case TOK_SYM_GEQ:
      return lhs >= rhs;
    case TOK_SYM_NEQ:
      return lhs != rhs;
    case TOK_SYM_EQ:
      return lhs == rhs;
    case '&':
      return lhs & rhs;
    case '|':
      return lhs | rhs;
    case '^':
      return lhs ^ rhs;
    case TOK_SYM_LOGIC_AND:
      return lhs && rhs;
    case TOK_SYM_LOGIC_OR:
      return lhs || rhs;
    case '<':
      return lhs < rhs;
    case '>':
      return lhs > rhs;
    case ',':
      return rhs;
    default:
      log_panic(
          "unexpected binop type when evaluating constant int expression: %s",
          lexer_token_get_str(expr->binop.op));
    }
  }
  case ast_expr_ternary: {
    return parser_expr_eval_const_int(expr->ternary.cond)
               ? parser_expr_eval_const_int(expr->ternary._t)
               : parser_expr_eval_const_int(expr->ternary._f);
  }
  case ast_expr_unary: {
    switch (expr->unary.op) {
    case '+':
      return +parser_expr_eval_const_int(expr->unary.expr);
    case '-':
      return -parser_expr_eval_const_int(expr->unary.expr);
    case '~':
      return ~parser_expr_eval_const_int(expr->unary.expr);
    case '!':
      return !parser_expr_eval_const_int(expr->unary.expr);
    case TOK_KW_SIZEOF:
    case '&':
    case '[':
    case '.':
      BUILDING();
    default:
      log_panic(
          "unexpected unary type when evaluating constant int expression: %s",
          lexer_token_get_str(expr->unary.op));
    }
  }
  case ast_expr_typecast: {
    BUILDING();
  }
  default:
    log_panic(
        "unexpected ast node type when checking constant int expression: %s",
        convert_repr_ast_type(expr->type));
  }
  return false;
}

bool parser_expr_check_const_int(astn expr) {
  if (expr->type == ast_ref) {
    expr = expr->ref;
  }
  switch (expr->type) {
  case ast_enumerator:
    return true;
  case ast_expr_primary: {
    return expr->primary.type == TOK_LIT_INT ||
           expr->primary.type == TOK_LIT_UINT ||
           expr->primary.type == TOK_LIT_LONG ||
           expr->primary.type == TOK_LIT_ULONG ||
           expr->primary.type == TOK_LIT_CHAR;
  }
  case ast_expr_binop: {
    return parser_expr_check_const_int(expr->binop.lhs) &&
           parser_expr_check_const_int(expr->binop.rhs);
  }
  case ast_expr_ternary: {
    return parser_expr_check_const_int(expr->ternary.cond) &&
           parser_expr_check_const_int(expr->ternary._t) &&
           parser_expr_check_const_int(expr->ternary._f);
  }
  case ast_expr_unary: {
    switch (expr->unary.op) {
    case '+':
    case '-':
    case '~':
    case '!':
    case '&':
    case TOK_KW_SIZEOF:
      return true;
    case '[':
    case '.':
      BUILDING();
    default:
      log_panic(
          "unexpected unary type when checking constant int expression: %s",
          lexer_token_get_str(expr->unary.op));
    }
    break;
  }
  case ast_expr_typecast: {
    return parser_expr_check_const_int(expr->typecast.expr);
  }
  default:
    log_panic(
        "unexpected ast node type when checking constant int expression: %s",
        convert_repr_ast_type(expr->type));
  }
  return false;
}

slist parser_gen_symtab(parser parser) {
  parser_symtab_remove_weak_symbols(&parser->symtab);
  parser_symtab_reorder_strong_symbols(&parser->symtab);
  return &parser->symtab;
}
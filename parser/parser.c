#include "parser.h"
#include "ast.h"
#include "convert/convert.h"
#include "grammar.h"
#include "lexer.h"
#include "log/log.h"
#include "macro/macro.h"
#include "sds/sds.h"
#include "slist/slist.h"
#include <stdbool.h>
int parser_consume(parser parser) {
  return parser->current_token = lexer_next_token(parser->lexer);
}

void parser_from_lexer(parser parser, struct lexer *lexer) {
  parser->lexer = malloc(sizeof(struct lexer));
  lexer_snapshot(parser->lexer, lexer);
  parser_consume(parser);
  slist_init(&parser->idtab);
  slist_init(&parser->tagtab);
  slist_init(&parser->symtab);
  parser->interruptable_scope = parser->current_function_scope = NULL;
  parser->uidcnt = 0;
}

void parser_snapshot(parser _new, parser _old) {
  *_new = *_old;
  _new->lexer = malloc(sizeof(struct lexer));
  lexer_snapshot(_new->lexer, _old->lexer);
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
                 lexer_token_to_string(token),
                 lexer_token_to_string(parser->current_token));
  return 0;
}

static struct ast_node __parser_scope_fence,
    *__parser_scope_fence_ptr = &__parser_scope_fence;
void parser_push_scope(parser parser) {
  log_debug("push scope at line %ld", parser->lexer->ln);
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

static inline astn __parser_cmp_astn_ident(astn data, sds ident) {
  if (0 == sdscmp(ast_declaration_ident(data), ident)) {
    return data;
  }
  return NULL;
}

astn parser_find_ident_in_all_scope_table(sds ident, slist tab) {
  astn data;
  slist_foreach(tab, data) {
    if (data == __parser_scope_fence_ptr) {
      continue;
    }
    astn r = __parser_cmp_astn_ident(data, ident);
    if (r) {
      return r;
    }
  }
  return NULL;
}

astn parser_find_ident_in_current_scope_table(sds ident, slist tab) {
  astn data;
  slist_foreach(tab, data) {
    if (data == __parser_scope_fence_ptr) {
      break;
    }
    astn r = __parser_cmp_astn_ident(data, ident);
    if (r) {
      return r;
    }
  }
  return NULL;
}

void parser_add_symbol_to_current_scope_table(astn n, slist tab) {
  log_debug("add declaration %s to current scope", ast_declaration_ident(n));
  slist_add_head(tab, n);
}

astn parser_get_typedef_by_type_name(parser parser, sds ident) {
  astn d = parser_find_ident_in_all_scope_table(ident, &parser->idtab);
  if (d && g_is_declaration_typedef(d)) {
    return d;
  }
  return NULL;
}

bool parser_is_current_block_global(parser parser) {
  // find NULL in the idtab
  return parser->current_function_scope == NULL;
}

/**
 * @brief Add a declaration to the symtab, usually used for merge extern symbol and symbol definition in codegen stage
 * 
 * @param parser 
 * @param n 
 */
void parser_add_to_symtab(parser parser, astn n) {
  assert(n->type == ast_declaration);
  if (g_get_declaration_specifier(n)->ctype.storage == TOK_KW_EXTERN) {
    log_debug("add weak symbol to symtab: %s", n->declaration.ident);
    slist_add_tail(&parser->symtab, n);
  } else {
    log_debug("add strong symbol to symtab: %s", n->declaration.ident);
    slist_add_head(&parser->symtab, n);
  }
}

/**
 * @brief add declaration to the current scope table,
 * set the uid for the declaration
 * 
 * @param parser 
 * @param n 
 */
void parser_declare_new_symbol(parser parser, astn n) {
  assert(n->type == ast_declaration);
  astn decl_specs = g_get_declaration_specifier(n);
  if (n->declaration.ident == NULL) {
    log_debug("this is an abstract declarator, skipping declaration");
    return;
  }
  astn existing_symbol = parser_find_ident_in_current_scope_table(
      n->declaration.ident, &parser->idtab);
  if (existing_symbol && decl_specs->ctype.storage == TOK_KW_EXTERN) {
    log_debug("multiple extern declaration, do nothing: %s",
              n->declaration.ident);
    return;
  } else if (existing_symbol && decl_specs->ctype.storage != TOK_KW_EXTERN &&
             g_get_declaration_specifier(existing_symbol)->ctype.storage !=
                 TOK_KW_EXTERN) {
    compiler_error(parser->lexer, "redefined symbol %s", n->declaration.ident);
  }

  n->declaration.uid = parser_get_uid(parser);
  parser_add_symbol_to_current_scope_table(n, &parser->idtab);
  if (parser_is_current_block_global(parser) ||
      decl_specs->ctype.storage == TOK_KW_EXTERN) {
    // we need add the extern symbol which is defined in block scope to symtab
    parser_add_to_symtab(parser, n);
  }
}

void parser_declare_new_enumerator(parser parser, astn n) {
  assert(n->type == ast_enumerator);
  if (n->enumerator.ident == NULL) {
    log_debug("this is an abstract enumerator, skipping");
    return;
  }
  astn existing_sym = parser_find_ident_in_current_scope_table(
      n->enumerator.ident, &parser->idtab);
  if (existing_sym) {
    compiler_error(parser->lexer, "redefined enumerator %s",
                   n->enumerator.ident);
  }
  n->enumerator.uid = parser_get_uid(parser);
  parser_add_symbol_to_current_scope_table(n, &parser->idtab);
}
/**
 * @brief declare a new struct/union/enum tag
 * 
 * @param parser 
 * @param n 
 */
void parser_declare_new_tag(parser parser, astn n) {
  if (n->type == ast_struct_union_declaration) {
    if (n->struct_union_declaration.ident == NULL) {
      log_debug("this is an abstract struct declarator, skipping declaration");
      return;
    }
    astn existing_tag = parser_find_ident_in_current_scope_table(
        n->struct_union_declaration.ident, &parser->tagtab);
    if (existing_tag) {
      compiler_error(parser->lexer, "redefined  tag with identifier %s",
                     n->struct_union_declaration.ident);
    }
    n->struct_union_declaration.uid = parser_get_uid(parser);
  } else {
    assert(n->type == ast_enumeration);
    if (n->enumeration.ident == NULL) {
      log_debug("this is an abstract enumeration, skipping declaration");
      return;
    }
    astn existing_tag = parser_find_ident_in_current_scope_table(
        n->enumeration.ident, &parser->tagtab);
    if (existing_tag) {
      compiler_error(parser->lexer, "redefined tag with identifier %s",
                     n->enumeration.ident);
    }
    n->enumeration.uid = parser_get_uid(parser);
  }

  parser_add_symbol_to_current_scope_table(n, &parser->tagtab);
}

size_t parser_get_uid(parser parser) {
  log_debug("allocating uid %ld", parser->uidcnt);
  return parser->uidcnt++;
}

void parser_unfold_type_chain(parser parser, slist type_chain) {
  astn tail = slist_peek_tail(type_chain);
  assert(tail->type == ast_ctype);
  enum type_qualifier qual = tail->ctype.qualifier;
  enum tok_type storage = tail->ctype.storage;
  if (tail->ctype.type == TOK_KW_TYPEDEF) {
    astn n = tail->ctype.user_defined_type;
    assert(n->type == ast_ref);
    n = n->ref;
    ast_free(slist_pop_tail(type_chain));
    astn t;
    slist typedef_typechain = &n->declaration.type_chain;
    slist_foreach(typedef_typechain, t) {
      slist_add_tail(type_chain, ast_copy(t));
    }
    // reset typedef
    n = slist_peek_tail(type_chain);
    assert(n->type == ast_ctype && n->ctype.storage == TOK_KW_TYPEDEF);
    n->ctype.qualifier |= qual;
    n->ctype.storage = storage;
  }
}
/**
 * @brief remove repeated weak symbols from symtab, 
 * 
 * @param symtab 
 * @param until 
 * @return true 
 * @return false 
 */
static bool __parser_symtab_symbol_exist_until(slist symtab, astn until) {
  sds id = until->declaration.ident;
  astn n;
  slist_foreach(symtab, n) {
    assert(n->type == ast_declaration);
    // traverse until extern symbol
    if (n == until) {
      break;
    }

    if (sdscmp(n->declaration.ident, id) == 0) {
      return true;
    }
  }
  return false;
}

void parser_symtab_remove_weak_symbols(slist symtab) {
  astn n;
  slist_foreach(symtab, n) {
    if (g_get_declaration_specifier(n)->ctype.storage != TOK_KW_EXTERN) {
      continue;
    }
    if (__parser_symtab_symbol_exist_until(symtab, n)) {
      log_debug("remove weak symbol from symtab: %s", n->declaration.ident);
      slist_remove(symtab, n);
    }
  }
}

/**
 * @brief the order of the strong symbol is reversed, so we need to reorder it
 * 
 */
slist parser_reorder_strong_symbols(slist symtab) {
  struct slist new_symtab;
  slist_init(&new_symtab);
  astn n;
  slist_foreach(symtab, n) {
    if (g_get_declaration_specifier(n)->ctype.storage == TOK_KW_EXTERN) {
      log_debug("reorder: add extern symbol to new symtab: %s",
                n->declaration.ident);
      slist_add_tail(&new_symtab, n);
    } else {
      log_debug("reorder: add strong symbol to new symtab: %s",
                n->declaration.ident);
      slist_add_head(&new_symtab, n);
    }
  }
  slist_free(symtab);
  *symtab = new_symtab;
  return symtab;
}

long parser_eval_const_int_expr(astn expr) {
  assert(parser_check_constant_int_expr(expr));
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
          lexer_token_to_string(expr->primary.type));
    }
  }
  case ast_expr_binop: {
    long long lhs = parser_eval_const_int_expr(expr->binop.lhs);
    long long rhs = parser_eval_const_int_expr(expr->binop.rhs);
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
          lexer_token_to_string(expr->binop.op));
    }
  }
  case ast_expr_ternary: {
    return parser_eval_const_int_expr(expr->ternary.cond)
               ? parser_eval_const_int_expr(expr->ternary._t)
               : parser_eval_const_int_expr(expr->ternary._f);
  }
  case ast_expr_unary: {
    switch (expr->unary.op) {
    case '+':
      return +parser_eval_const_int_expr(expr->unary.expr);
    case '-':
      return -parser_eval_const_int_expr(expr->unary.expr);
    case '~':
      return ~parser_eval_const_int_expr(expr->unary.expr);
    case '!':
      return !parser_eval_const_int_expr(expr->unary.expr);
    case TOK_KW_SIZEOF:
    case '&':
    case '[':
    case '.':
      BUILDING();
    default:
      log_panic(
          "unexpected unary type when evaluating constant int expression: %s",
          lexer_token_to_string(expr->unary.op));
    }
  }
  case ast_expr_typecast: {
    BUILDING();
  }
  default:
    log_panic(
        "unexpected ast node type when checking constant int expression: %s",
        convert_ast_type_enum_to_repr(expr->type));
  }
  return false;
}

bool parser_check_constant_int_expr(astn expr) {
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
    return parser_check_constant_int_expr(expr->binop.lhs) &&
           parser_check_constant_int_expr(expr->binop.rhs);
  }
  case ast_expr_ternary: {
    return parser_check_constant_int_expr(expr->ternary.cond) &&
           parser_check_constant_int_expr(expr->ternary._t) &&
           parser_check_constant_int_expr(expr->ternary._f);
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
          lexer_token_to_string(expr->unary.op));
    }
    break;
  }
  case ast_expr_typecast: {
    return parser_check_constant_int_expr(expr->typecast.expr);
  }
  default:
    log_panic(
        "unexpected ast node type when checking constant int expression: %s",
        convert_ast_type_enum_to_repr(expr->type));
  }
  return false;
}

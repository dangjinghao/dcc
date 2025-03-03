#include "parser.h"
#include "ast.h"
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
  parser->global_block = parser->interruptable_block =
      parser->current_function_block = NULL;
  parser->global_uid = parser->local_uid = 0;
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
  compiler_error(parser->lexer, "Expected token `%s`, got `%s`",
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
    log_debug("pop declaration `%s`", r->declaration.ident);
  }
}

astn parser_find_declaration_in_all_scope_table(sds ident, slist tab) {
  astn data;
  slist_foreach(tab, data) {
    if (data == __parser_scope_fence_ptr) {
      continue;
    }
    if (data->type == ast_declaration &&
        sdscmp(data->declaration.ident, ident) == 0) {
      return data;
    } else if (data->type == ast_struct_union_declaration &&
               sdscmp(data->struct_union_declaration.ident, ident) == 0) {
      return data;
    }
  }
  return NULL;
}

astn parser_find_declaration_in_current_scope_table(sds ident, slist tab) {
  astn data;
  slist_foreach(tab, data) {
    if (data == __parser_scope_fence_ptr) {
      break;
    }
    if (data->type == ast_declaration &&
        sdscmp(data->declaration.ident, ident) == 0) {
      return data;
    } else if (data->type == ast_struct_union_declaration &&
               sdscmp(data->struct_union_declaration.ident, ident) == 0) {
      return data;
    }
  }
  return NULL;
}

void parser_add_declaration_to_current_scope_table(astn n, slist tab) {
  if (n->type == ast_declaration) {
    log_debug("add declaration `%s` to current scope", n->declaration.ident);

  } else if (n->type == ast_struct_union_declaration) {
    log_debug("add struct declaration `%s` to current scope",
              n->struct_union_declaration.ident);
  }
  slist_add_head(tab, n);
}

bool parser_check_constant_expr(astn expr) {
  return true;
  BUILDING();
}

astn parser_get_typedef_by_type_name(parser parser, sds ident) {
  astn d = parser_find_declaration_in_all_scope_table(ident, &parser->idtab);
  if (d && g_is_declaration_typedef(d)) {
    return d;
  }
  return NULL;
}

bool parser_is_current_block_global(parser parser) {
  // find NULL in the idtab
  return parser->current_function_block == NULL;
}

static astn __parser_declaration_ident_exists(slist declaration_list, astn n) {
  astn ref;
  slist_foreach(declaration_list, ref) {
    if (sdscmp(ref->declaration.ident, n->declaration.ident) == 0) {
      return ref;
    }
  }
  return NULL;
}
/**
 * @brief Add a declaration to the symtab, usually used for re-locate extern symbol in codegen stage
 * 
 * @param parser 
 * @param n 
 */
void parser_add_to_symtab(parser parser, astn n) {
  assert(n->type == ast_declaration);
  astn exists = __parser_declaration_ident_exists(&parser->symtab, n);
  if (exists &&
      g_get_declaration_specifier(exists)->ctype.storage == TOK_KW_EXTERN) {
    log_debug("found defined extern declaration in symtab, remove: %s",
              n->declaration.ident);
    slist_remove(&parser->symtab, exists);
  } else if (exists && g_get_declaration_specifier(exists)->ctype.storage !=
                           TOK_KW_EXTERN) {
    log_debug(
        "there has been a strong symbol `%s` in symtab, just ignore adding",
        n->declaration.ident);
    return;
  }
  slist_add_head(&parser->symtab, n);
}

/**
 * @brief add declaration to the current scope table,
 * set the uid for the declaration
 * 
 * @param parser 
 * @param n 
 */
void parser_declare_new_symbol(parser parser, astn n) {
  astn decl_specs = g_get_declaration_specifier(n);
  if (n->declaration.ident == NULL) {
    log_debug("this is an abstract declarator, skipping declaration");
    return;
  }
  astn exists_declaration = parser_find_declaration_in_current_scope_table(
      n->declaration.ident, &parser->idtab);
  if (exists_declaration && decl_specs->ctype.storage == TOK_KW_EXTERN) {
    log_debug("multiple extern declaration, do nothing: %s",
              n->declaration.ident);
    return;
  } else if (exists_declaration && decl_specs->ctype.storage != TOK_KW_EXTERN &&
             g_get_declaration_specifier(exists_declaration)->ctype.storage !=
                 TOK_KW_EXTERN) {
    compiler_error(parser->lexer, "redefined symbol `%s`",
                   n->declaration.ident);
  }
  parser_set_declaration_uid(n, parser);
  parser_add_declaration_to_current_scope_table(n, &parser->idtab);
  if (parser_is_current_block_global(parser) ||
      decl_specs->ctype.storage == TOK_KW_EXTERN) {
    // we need add the extern symbol which is defined in block scope to symtab
    parser_add_to_symtab(parser, n);
  }
}
/**
 * @brief declare a new struct/union/enum tag
 * 
 * @param parser 
 * @param n 
 */
void parser_declare_new_tag(parser parser, astn n) {
  if (n->struct_union_declaration.ident == NULL) {
    log_debug("this is an abstract struct declarator, skipping declaration");
    return;
  }
  astn exists_declaration = parser_find_declaration_in_current_scope_table(
      n->struct_union_declaration.ident, &parser->tagtab);
  if (exists_declaration) {
    compiler_error(parser->lexer, "redefined struct/union with identifier `%s`",
                   n->struct_union_declaration.ident);
  }
  parser_add_declaration_to_current_scope_table(n, &parser->tagtab);
}

size_t parser_get_local_uid(parser parser) {
  log_debug("get local uid %ld", parser->local_uid);
  return parser->local_uid++;
}
size_t parser_get_global_uid(parser parser) {
  log_debug("get global uid %ld", parser->global_uid);
  return parser->global_uid++;
}

void parser_set_declaration_uid(astn n, parser parser) {
  if (n->declaration.ident) {
    if (parser_is_current_block_global(parser)) {
      n->declaration.uid = parser_get_global_uid(parser);
    } else {
      n->declaration.uid = parser_get_local_uid(parser);
    }
  } else {
    log_debug("this is an abstract declarator, skipping allocate uid");
  }
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
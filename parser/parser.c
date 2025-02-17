#include "parser.h"
#include "ast.h"
#include "dynarray/dynarray.h"
#include "grammar.h"
#include "lexer.h"
#include "macro/macro.h"
#include "sds/sds.h"
int parser_consume(parser parser) {
  return parser->current_token = lexer_next_token(parser->lexer);
}

void parser_from_lexer(parser parser, struct lexer *lexer) {
  parser->lexer = malloc(sizeof(struct lexer));
  lexer_snapshot(parser->lexer, lexer);
  parser_consume(parser);
  dynarray_default(&parser->idtab, sizeof(struct declaration *));
  dynarray_default(&parser->tagtab, sizeof(struct declaration *));
}

void parser_snapshot(parser _new, parser _old) {
  _new->lexer = malloc(sizeof(struct lexer));
  lexer_snapshot(_new->lexer, _old->lexer);
  _new->current_token = _old->current_token;
  // bad hack
  if (_old->current_token == TOK_IDENT ||
      _old->current_token == TOK_LIT_STRING) {
    _new->lexer->lex_token._ident = sdsdup(_old->lexer->lex_token._ident);
  }
  dynarray_copy(&_new->idtab, &_old->idtab);
  dynarray_copy(&_new->tagtab, &_old->tagtab);
}

void parser_destory(parser parser) {
  // bad hack
  if (parser->current_token == TOK_IDENT ||
      parser->current_token == TOK_LIT_STRING) {
    sdsfree(parser->lexer->lex_token._ident);
  }
  dynarray_free(&parser->idtab);
  dynarray_free(&parser->tagtab);
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

void parser_push_scope(parser parser) {
  void *p = NULL;
  dynarray_add(&parser->idtab, &p);
  dynarray_add(&parser->tagtab, &p);
}

void parser_pop_scope(parser parser) {
  struct declaration **ref;
  dynarray_foreach_reverse(&parser->idtab, ref) {
    if (*ref == NULL) {
      // we cannot sure the ref content after pop, so those 2 branchs should not be merged
      dynarray_pop(&parser->idtab, NULL);
      break;
    } else {
      dynarray_pop(&parser->idtab, NULL);
    }
  }
}

astn parser_find_declaration_in_all_scope_table(sds ident, dynarray tab) {
  astn *ref;
  dynarray_foreach_reverse(tab, ref) {
    if (*ref == NULL) {
      continue;
    } else if (sdscmp((*ref)->ident, ident) == 0) {
      return *ref;
    }
  }
  return NULL;
}

astn parser_find_declaration_in_current_scope_table(sds ident, dynarray tab) {
  astn *ref;
  dynarray_foreach_reverse(tab, ref) {
    if (*ref == NULL) {
      break;
    } else if (sdscmp((*ref)->ident, ident) == 0) {
      return *ref;
    }
  }
  return NULL;
}

void parser_add_declaration_to_current_scope_table(astn decl,
                                                   dynarray tab) {
  dynarray_add(tab, &decl);
}

bool parser_check_constant_expr(astn expr) {
  return true;
  BUILDING();
}

astn parser_get_typedef(parser parser, sds ident) {
  astn d = parser_find_declaration_in_all_scope_table(ident, &parser->idtab);
  if (d && g_is_declaration_typedef(d)) {
    return d;
  }
  return NULL;
}

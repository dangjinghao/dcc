#include "parser.h"
#include "ast.h"
#include "grammar.h"
#include "lexer.h"
#include "macro/macro.h"
#include "sds/sds.h"
#include "slist/slist.h"
int parser_consume(parser parser) {
  return parser->current_token = lexer_next_token(parser->lexer);
}

void parser_from_lexer(parser parser, struct lexer *lexer) {
  parser->lexer = malloc(sizeof(struct lexer));
  lexer_snapshot(parser->lexer, lexer);
  parser_consume(parser);
  slist_init(&parser->idtab);
  slist_init(&parser->tagtab);
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
  slist_copy(&_new->idtab, &_old->idtab);
  slist_copy(&_new->tagtab, &_old->tagtab);
}

void parser_destory(parser parser) {
  // bad hack
  if (parser->current_token == TOK_IDENT ||
      parser->current_token == TOK_LIT_STRING) {
    sdsfree(parser->lexer->lex_token._ident);
  }
  slist_free(&parser->idtab);
  slist_free(&parser->tagtab);
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
  slist_add_head(&parser->idtab, NULL);
  slist_add_head(&parser->tagtab, NULL);
}

void parser_pop_scope(parser parser) {
  struct declaration *data;
  slist_foreach(&parser->idtab, data) {
    slist_pop_head(&parser->idtab);
    if (data == NULL) {
      break;
    }
  }
}

astn parser_find_declaration_in_all_scope_table(sds ident, slist tab) {
  astn data;
  slist_foreach(tab, data) {
    if (data == NULL) {
      continue;
    }
    if (sdscmp(data->ident, ident) == 0) {
      return data;
    }
  }
  return NULL;
}

astn parser_find_declaration_in_current_scope_table(sds ident, slist tab) {
  astn data;
  slist_foreach(tab, data) {
    if (data == NULL) {
      break;
    }
    if (sdscmp(data->ident, ident) == 0) {
      return data;
    }
  }
  return NULL;
}

void parser_add_declaration_to_current_scope_table(astn decl, slist tab) {
  slist_add_head(tab, decl);
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

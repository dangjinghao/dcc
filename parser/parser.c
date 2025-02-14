#include "parser.h"
#include "ast.h"
#include "dynarray/dynarray.h"
#include "lexer.h"
#include "sds/sds.h"
int parser_consume(parser parser) {
  return parser->current_token = lexer_next_token(parser->lexer);
}

void parser_from_lexer(parser parser, struct lexer *lexer) {
  parser->lexer = lexer;
  parser_consume(parser);
  dynarray_default(&parser->idtab, sizeof(struct declaration *));
  dynarray_default(&parser->tagtab, sizeof(struct declaration *));
}

void parser_snapshot(parser _new, parser _old) {
  lexer_snapshot(_new->lexer, _old->lexer);
  _new->current_token = _old->current_token;
  dynarray_copy(&_new->idtab, &_old->idtab);
  dynarray_copy(&_new->tagtab, &_old->tagtab);
}

void parser_free(parser parser) {
  dynarray_free(&parser->idtab);
  dynarray_free(&parser->tagtab);
}

int parser_consume_with(parser parser, int token) {
  if (parser->current_token == token) {
    if (token == TOK_LIT_STRING || token == TOK_IDENT) {
      sdsfree(parser->lexer->lex_token._str);
    }
    return parser_consume(parser);
  }
  compiler_error(parser->lexer, "Expected token %s, got %s",
                 lexer_token_to_string(token),
                 lexer_token_to_string(parser->current_token));
  return 0;
}
/**
 * @brief Free the ast node, if the content is in heap memory, it would be skipped.
 * 
 * @param node 
 */
void parser_free_ast(astn node) {
  if (!node)
    return;
  if (node->type == ast_expr_unary) {
    parser_free_ast(node->unary.expr);
  } else if (node->type == ast_expr_binop) {
    parser_free_ast(node->binop.lhs);
    parser_free_ast(node->binop.rhs);
  } else if (node->type == ast_expr_ternary) {
    parser_free_ast(node->ternary.cond);
    parser_free_ast(node->ternary._t);
    parser_free_ast(node->ternary._f);
  } else if (node->type == ast_expr_primary) {
    if (node->primary.type == TOK_LIT_STRING ||
        node->primary.type == TOK_IDENT) {
      sdsfree(node->primary.v._str);
    }
  }

  free(node);
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

struct declaration *parser_find_in_all_scope_table(sds ident, dynarray tab) {
  struct declaration **ref;
  dynarray_foreach_reverse(tab, ref) {
    if (*ref == NULL) {
      continue;
    } else if (sdscmp((*ref)->ident, ident) == 0) {
      return *ref;
    }
  }
  return NULL;
}

struct declaration *parser_find_in_current_scope_table(sds ident,
                                                       dynarray tab) {
  struct declaration **ref;
  dynarray_foreach_reverse(tab, ref) {
    if (*ref == NULL) {
      break;
    } else if (sdscmp((*ref)->ident, ident) == 0) {
      return *ref;
    }
  }
  return NULL;
}

void parser_add_to_current_scope_table(struct declaration *decl, dynarray tab) {
  dynarray_add(tab, &decl);
}
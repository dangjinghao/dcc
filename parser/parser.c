#include "parser.h"
#include "ast.h"
#include "dynarray/dynarray.h"
#include "lexer.h"
#include "log/log.h"
#include "macro/macro.h"
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

void parser_destory(parser parser) {
  dynarray_free(&parser->idtab);
  dynarray_free(&parser->tagtab);
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
/**
 * @brief Free the ast node and its children and contents
 * 
 * @param node 
 */
void ast_free(astn node) {
  if (!node)
    return;
  switch (node->type) {
  case ast_expr_unary: {
    ast_free(node->unary.expr);
    break;
  }
  case ast_expr_binop: {
    ast_free(node->binop.lhs);
    ast_free(node->binop.rhs);
    break;
  }
  case ast_expr_ternary: {
    ast_free(node->ternary.cond);
    ast_free(node->ternary._t);
    ast_free(node->ternary._f);
    break;
  }
  case ast_expr_primary: {
    if (node->primary.type == TOK_LIT_STRING) {
      sdsfree(node->primary.v._str);
    }
    break;
  }
  case ast_ident: {
    sdsfree(node->ident);
    break;
  }
  case ast_declaration: {
    astn *ref;
    dynarray_foreach(node->declaration.type_chain, ref) { ast_free(*ref); }
    dynarray_free(node->declaration.type_chain);
    free(node->declaration.type_chain);
    ast_free(node->declaration.extdata);
    sdsfree(node->declaration.ident);
    break;
  }
  case ast_block: {
    astn *ref;
    dynarray_foreach(node->block.stmts, ref) { ast_free(*ref); }
    dynarray_free(node->block.stmts);
    free(node->block.stmts);
    break;
  }
  case ast_ctype: {
    ast_free(node->ctype.user_defined_type);
    break;
  }
  case ast_labeled_statement: {
    ast_free(node->labeled_statement.stmt);
    if (node->labeled_statement.label_value)
      ast_free(node->labeled_statement.label_value);
    break;
  }
  default: {
    log_error("Wrong ast type %d", node->type);
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

bool parser_check_constant_expr(astn expr) {
  return true;
  BUILDING();
}

astn ast_new(enum ast_type type) {
  struct ast_node *node = calloc(1, sizeof(struct ast_node));
  node->type = type;
  switch (type) {
  case ast_declaration:
    node->declaration.type_chain = calloc(1, sizeof(struct dynarray));
    dynarray_default(node->declaration.type_chain, sizeof(astn));
    break;
  case ast_block:
    node->block.stmts = calloc(1, sizeof(struct dynarray));
    dynarray_default(node->block.stmts, sizeof(astn));
    break;
  default:
    break;
  }
  return node;
}

astn ast_new_empty_statement() {
  astn stmt = ast_new(ast_expr_primary);
  stmt->primary.type = TOK_EOF;
  return stmt;
}

astn ast_copy(astn n) {
  if (!n) {
    return NULL;
  }
  astn new = ast_new(n->type);
  switch (n->type) {
  case ast_expr_unary:
    new->unary.op = n->unary.op;
    new->unary.postfix = n->unary.postfix;
    new->unary.expr = ast_copy(n->unary.expr);
    break;
  case ast_expr_binop:
    new->binop.op = n->binop.op;
    new->binop.lhs = ast_copy(n->binop.lhs);
    new->binop.rhs = ast_copy(n->binop.rhs);
    break;
  case ast_expr_ternary:
    new->ternary.cond = ast_copy(n->ternary.cond);
    new->ternary._t = ast_copy(n->ternary._t);
    new->ternary._f = ast_copy(n->ternary._f);
    break;
  case ast_expr_primary:
    new->primary.type = n->primary.type;
    new->primary.v = n->primary.v;
    if (n->primary.type == TOK_LIT_STRING) {
      new->primary.v._str = sdsdup(n->primary.v._str);
    }
    break;
  case ast_ident:
    new->ident = sdsdup(n->ident);
    break;
  case ast_declaration:
    new->declaration.ident = sdsdup(n->declaration.ident);
    new->declaration.extdata = ast_copy(n->declaration.extdata);
    astn *ref;
    dynarray_foreach(n->declaration.type_chain, ref) {
      astn copy = ast_copy(*ref);
      dynarray_add(new->declaration.type_chain, &copy);
    }
    break;
  case ast_block: {
    astn *ref;
    dynarray_foreach(n->block.stmts, ref) {
      astn copy = ast_copy(*ref);
      dynarray_add(new->block.stmts, &copy);
    }
    break;
  }
  case ast_ctype:
    new->ctype.qualifier = n->ctype.qualifier;
    new->ctype.type = n->ctype.type;
    new->ctype.signint = n->ctype.signint;
    new->ctype.storage = n->ctype.storage;
    new->ctype.user_defined_type = ast_copy(n->ctype.user_defined_type);
    break;
  case ast_labeled_statement:
    new->labeled_statement.type = n->labeled_statement.type;
    new->labeled_statement.label_value =
        ast_copy(n->labeled_statement.label_value);
    new->labeled_statement.stmt = ast_copy(n->labeled_statement.stmt);
    break;
  default:
    log_error("Wrong ast type %d", n->type);
  }
  return new;
}

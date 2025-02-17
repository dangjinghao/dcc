#include "ast.h"
#include "log/log.h"

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
      new->ctype.user_defined_type = n->ctype.user_defined_type;
      break;
    case ast_labeled_statement:
      new->labeled_statement.type = n->labeled_statement.type;
      new->labeled_statement.label_value =
          ast_copy(n->labeled_statement.label_value);
      new->labeled_statement.stmt = ast_copy(n->labeled_statement.stmt);
      break;
    default:
      log_panic("Wrong ast type %d", n->type);
    }
    return new;
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
    break;
  }
  case ast_labeled_statement: {
    ast_free(node->labeled_statement.stmt);
    if (node->labeled_statement.label_value)
      ast_free(node->labeled_statement.label_value);
    break;
  }
  default: {
    log_panic("Wrong ast type %d", node->type);
  }
  }

  free(node);
}

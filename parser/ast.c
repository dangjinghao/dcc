#include "ast.h"
#include "slist/slist.h"

astn ast_new(enum ast_type type) {
  struct astn *node = calloc(1, sizeof(struct astn));
  node->type = type;
  switch (type) {
  case ast_declaration:
    slist_init(&node->declaration.type_chain);
    break;
  case ast_expr_typecast:
    slist_init(&node->typecast.type_chain);
    break;
  case ast_struct_union_declaration:
    slist_init(&node->struct_union_declaration.member_declarations);
    break;
  case ast_parameters:
    slist_init(&node->parameters.list);
    break;
  case ast_block:
    slist_init(&node->block.list);
    break;
  case ast_trans_unit:
    slist_init(&node->trans_unit.list);
    break;
  case ast_arguments:
    slist_init(&node->arguments.list);
    break;
  case ast_initializer_list:
    slist_init(&node->initializer_list.list);
    break;
  case ast_enumeration:
    slist_init(&node->enumeration.enumerators);
    break;
  default:
    break;
  }
  return node;
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
    new->unary.extdata = ast_copy(n->unary.extdata);
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
    new->declaration.scope_ref = n->declaration.scope_ref;
    new->declaration.uid = n->declaration.uid;
    astn ref;
    slist_foreach(&n->declaration.type_chain, ref) {
      astn copy = ast_copy(ref);
      slist_add_tail(&new->declaration.type_chain, copy);
    }
    break;
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
  case ast_expr_typecast: {
    astn ref;
    new->typecast.expr = ast_copy(n->typecast.expr);
    slist_foreach(&n->typecast.type_chain, ref) {
      astn copy = ast_copy(ref);
      slist_add_tail(&new->typecast.type_chain, copy);
    }
    break;
  }
  case ast_struct_union_declaration: {
    new->struct_union_declaration.ident =
        n->struct_union_declaration.ident
            ? sdsdup(n->struct_union_declaration.ident)
            : NULL;
    new->struct_union_declaration.uid = n->struct_union_declaration.uid;
    astn ref;
    slist_foreach(&n->struct_union_declaration.member_declarations, ref) {
      astn copy = ast_copy(ref);
      slist_add_tail(&new->struct_union_declaration.member_declarations, copy);
    }
    break;
  }
  case ast_parameters: {
    astn ref;
    slist_foreach(&n->parameters.list, ref) {
      astn copy = ast_copy(ref);
      slist_add_tail(&new->parameters.list, copy);
    }
    break;
  }
  case ast_block: {
    astn ref;
    slist_foreach(&n->block.list, ref) {
      astn copy = ast_copy(ref);
      slist_add_tail(&new->block.list, copy);
    }
    break;
  }
  case ast_trans_unit: {
    astn ref;
    slist_foreach(&n->trans_unit.list, ref) {
      astn copy = ast_copy(ref);
      slist_add_tail(&new->trans_unit.list, copy);
    }
    break;
  }
  case ast_arguments: {
    astn ref;
    slist_foreach(&n->arguments.list, ref) {
      astn copy = ast_copy(ref);
      slist_add_tail(&new->arguments.list, copy);
    }
    break;
  }
  case ast_initializer: {
    new->initializer.init = ast_copy(n->initializer.init);
    break;
  }
  case ast_ref: {
    new->ref = n->ref;
    break;
  }
  case ast_initializer_list: {
    astn ref;
    slist_foreach(&n->initializer_list.list, ref) {
      astn copy = ast_copy(ref);
      slist_add_tail(&new->initializer_list.list, copy);
    }
    break;
  }
  case ast_jump_statement: {
    new->jump_statement.type = n->jump_statement.type;
    new->jump_statement.expr = ast_copy(n->jump_statement.expr);
    new->jump_statement.scope_ref = n->jump_statement.scope_ref;
    break;
  }
  case ast_enumeration: {
    new->enumeration.ident =
        n->enumeration.ident ? sdsdup(n->enumeration.ident) : NULL;
    new->enumeration.uid = n->enumeration.uid;
    astn ref;
    slist_foreach(&n->enumeration.enumerators, ref) {
      astn copy = ast_copy(ref);
      slist_add_tail(&new->enumeration.enumerators, copy);
    }
    break;
  }
  case ast_enumerator: {
    new->enumerator.ident = sdsdup(n->enumerator.ident);
    new->enumerator.value = n->enumerator.value;
    new->enumerator.uid = n->enumerator.uid;
    break;
  }
  case ast_iteration: {
    new->iteration.type = n->iteration.type;
    new->iteration.body = ast_copy(n->iteration.body);
    new->iteration.init = ast_copy(n->iteration.init);
    new->iteration.cond = ast_copy(n->iteration.cond);
    new->iteration.inc = ast_copy(n->iteration.inc);
    break;
  }
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
    ast_free(node->unary.extdata);
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
    astn ref;
    slist_foreach(&node->declaration.type_chain, ref) { ast_free(ref); }
    slist_free(&node->declaration.type_chain);
    ast_free(node->declaration.extdata);
    sdsfree(node->declaration.ident);
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

  case ast_expr_typecast: {
    astn ref;
    ast_free(node->typecast.expr);
    slist_foreach(&node->typecast.type_chain, ref) { ast_free(ref); }
    slist_free(&node->typecast.type_chain);
    break;
  }
  case ast_ref: {
    break;
  }
  case ast_struct_union_declaration: {
    astn ref;
    slist_foreach(&node->struct_union_declaration.member_declarations, ref) {
      ast_free(ref);
    }
    sdsfree(node->struct_union_declaration.ident);
    slist_free(&node->struct_union_declaration.member_declarations);
    break;
  }
  case ast_parameters: {
    astn ref;
    slist_foreach(&node->parameters.list, ref) { ast_free(ref); }
    slist_free(&node->parameters.list);
    break;
  }
  case ast_block: {
    astn ref;
    slist_foreach(&node->block.list, ref) { ast_free(ref); }
    slist_free(&node->block.list);
    break;
  }
  case ast_trans_unit: {
    astn ref;
    slist_foreach(&node->trans_unit.list, ref) { ast_free(ref); }
    slist_free(&node->trans_unit.list);
    break;
  }
  case ast_arguments: {
    astn ref;
    slist_foreach(&node->arguments.list, ref) { ast_free(ref); }
    slist_free(&node->arguments.list);
    break;
  }
  case ast_initializer: {
    ast_free(node->initializer.init);
    break;
  }
  case ast_initializer_list: {
    astn ref;
    slist_foreach(&node->initializer_list.list, ref) { ast_free(ref); }
    slist_free(&node->initializer_list.list);
    break;
  }
  case ast_jump_statement: {
    ast_free(node->jump_statement.expr);
    break;
  }
  case ast_enumeration: {
    astn ref;
    slist_foreach(&node->enumeration.enumerators, ref) { ast_free(ref); }
    slist_free(&node->enumeration.enumerators);
    sdsfree(node->enumeration.ident);
    break;
  }
  case ast_enumerator:
    sdsfree(node->enumerator.ident);
    break;
  case ast_iteration:
    ast_free(node->iteration.body);
    ast_free(node->iteration.init);
    ast_free(node->iteration.cond);
    ast_free(node->iteration.inc);
    break;
  }
  free(node);
}

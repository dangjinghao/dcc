#include "dcc.h"
#include <assert.h>
#include <stdlib.h>

static Node *new_node(NodeKind kind, Token *tok) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->tok = tok;
  return node;
}

static Node *new_num(Token *tok) {
  Node *node = new_node(ND_NUM, tok);
  assert(tok->ty);
  if (is_integer(tok->ty)) {
    node->val = tok->val;
    node->is_unsigned = tok->ty->is_unsigned;
  } else if (is_flonum(tok->ty)) {
    node->fval = tok->fval;
  } else {
    unreachable();
  }
  node->ty = tok->ty;
  return node;
}

Node *new_cast(Node *expr, Type *ty) {
  add_type(expr);

  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_CAST;
  node->tok = expr->tok;
  node->lhs = expr;
  node->ty = copy_type(ty);
  return node;
}

// primary = "(" "{" stmt+ "}" ")"
//         | "(" expr ")"
//         | "sizeof" "(" type-name ")"
//         | "sizeof" unary
//         | "_Alignof" "(" type-name ")"
//         | "_Alignof" unary
//         | "_Generic" generic-selection
//         | "__builtin_types_compatible_p" "(" type-name, type-name, ")"
//         | "__builtin_reg_class" "(" type-name ")"
//         | ident
//         | str
//         | num
static Node *primary(Token **rest, Token *tok) {

  if (tok->kind == TK_NUM) {
    Node *node = new_num(tok);
    *rest = tok->next;
    return node;
  }

  error_tok(tok, "expected a primary expression");
}

void parse(Token *tok) {
  while (tok->kind != TK_EOF) {
    Node *n = primary(&tok, tok);
  }
}
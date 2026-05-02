#include "dcc.h"
#include <stdlib.h>

Node *new_cast(Node *expr, Type *ty) {
  add_type(expr);

  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_CAST;
  node->tok = expr->tok;
  node->lhs = expr;
  node->ty = copy_type(ty);
  return node;
}
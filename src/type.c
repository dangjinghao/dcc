#include "dcc.h"
#include <stdalign.h>
#include <stdlib.h>

// basic type

// debug infomation
static Type _ty_void = {
    .kind = TY_VOID, .size = sizeof(void), .align = alignof(void)};
Type *ty_void = &_ty_void;

static Type _ty_bool = {
    .kind = TY_BOOL, .size = sizeof(bool), .align = alignof(bool)};
Type *ty_bool = &_ty_bool;

static Type _ty_char = {
    .kind = TY_CHAR, .size = sizeof(char), .align = alignof(char)};
Type *ty_char = &_ty_char;

static Type _ty_short = {
    .kind = TY_SHORT, .size = sizeof(short), .align = alignof(short)};
Type *ty_short = &_ty_short;

static Type _ty_int = {
    .kind = TY_INT, .size = sizeof(int), .align = alignof(int)};
Type *ty_int = &_ty_int;

static Type _ty_long = {
    .kind = TY_LONG, .size = sizeof(long), .align = alignof(long)};
Type *ty_long = &_ty_long;

static Type _ty_uchar = {.kind = TY_CHAR,
                         .size = sizeof(unsigned char),
                         .align = alignof(unsigned char),
                         .is_unsigned = true};
Type *ty_uchar = &_ty_uchar;

static Type _ty_ushort = {.kind = TY_SHORT,
                          .size = sizeof(unsigned short),
                          .align = alignof(unsigned short),
                          .is_unsigned = true};
Type *ty_ushort = &_ty_ushort;

static Type _ty_uint = {.kind = TY_INT,
                        .size = sizeof(unsigned int),
                        .align = alignof(unsigned int),
                        .is_unsigned = true};
Type *ty_uint = &_ty_uint;

static Type _ty_ulong = {.kind = TY_LONG,
                         .size = sizeof(unsigned long),
                         .align = alignof(unsigned long),
                         .is_unsigned = true};
Type *ty_ulong = &_ty_ulong;

static Type _ty_float = {
    .kind = TY_FLOAT, .size = sizeof(float), .align = alignof(float)};
Type *ty_float = &_ty_float;

static Type _ty_double = {
    .kind = TY_DOUBLE, .size = sizeof(double), .align = alignof(double)};
Type *ty_double = &_ty_double;

static Type _ty_ldouble = {.kind = TY_LDOUBLE,
                           .size = sizeof(long double),
                           .align = alignof(long double)};
Type *ty_ldouble = &_ty_ldouble;

static Type *new_type(TypeKind kind, int size, int align) {
  Type *ty = calloc(1, sizeof(Type));
  ty->kind = kind;
  ty->size = size;
  ty->align = align;
  return ty;
}

bool is_integer(Type *ty) {
  TypeKind k = ty->kind;
  return k == TY_BOOL || k == TY_CHAR || k == TY_SHORT || k == TY_INT ||
         k == TY_LONG || k == TY_ENUM;
}

bool is_flonum(Type *ty) {
  return ty->kind == TY_FLOAT || ty->kind == TY_DOUBLE ||
         ty->kind == TY_LDOUBLE;
}

bool is_numeric(Type *ty) { return is_integer(ty) || is_flonum(ty); }

// used in generic_selection and __builtin_types_compatible_p
bool is_compatible(Type *t1, Type *t2) {
  if (t1 == t2)
    return true;

  if (t1->origin)
    return is_compatible(t1->origin, t2);

  if (t2->origin)
    return is_compatible(t1, t2->origin);

  if (t1->kind != t2->kind)
    return false;

  switch (t1->kind) {
  case TY_CHAR:
  case TY_SHORT:
  case TY_INT:
  case TY_LONG:
    return t1->is_unsigned == t2->is_unsigned;
  case TY_FLOAT:
  case TY_DOUBLE:
  case TY_LDOUBLE:
    return true;
  case TY_PTR:
    return is_compatible(t1->base, t2->base);
  case TY_FUNC: {
    if (!is_compatible(t1->return_ty, t2->return_ty))
      return false;
    if (t1->is_variadic != t2->is_variadic)
      return false;

    Type *p1 = t1->params;
    Type *p2 = t2->params;
    while (p1 && p2) {
      if (!is_compatible(p1, p2))
        return false;
      p1 = p1->next;
      p2 = p2->next;
    }
    return p1 == NULL && p2 == NULL;
  }
  case TY_ARRAY: {
    if (!is_compatible(t1->base, t2->base))
      return false;
    return t1->array_len < 0 || t2->array_len < 0 ||
           t1->array_len == t2->array_len;
  }
  default:
    break;
  }
  return false;
}

Type *copy_type(Type *ty) {
  Type *ret = calloc(1, sizeof(Type));
  *ret = *ty;
  ret->origin = ty;
  return ret;
}

Type *pointer_to(Type *base) {
  Type *ty = new_type(TY_PTR, sizeof(void *), alignof(void *));
  ty->base = base;
  ty->is_unsigned = true;
  return ty;
}

Type *func_type(Type *ret_ty) {
  Type *ty = new_type(TY_FUNC, 1, 1);
  ty->return_ty = ret_ty;
  return ty;
}

Type *array_of(Type *base, int len) {
  Type *ty = new_type(TY_ARRAY, base->size * len, base->align);
  ty->base = base;
  ty->array_len = len;
  return ty;
}

Type *vla_of(Type *base, Node *len) {
  // VLA size and alignment cannot be determined at compile time.
  // Storing 8 as a placeholder is harmless; the actual size comes from
  // the expression tree.
  Type *ty = new_type(TY_VLA, 8, 8);
  ty->base = base;
  ty->vla_len = len;
  return ty;
}

Type *enum_type(void) {
  // Treat enum as int (simplified; standard allows larger underlying types)
  return new_type(TY_ENUM, sizeof(int), sizeof(int));
}

Type *struct_type(void) { return new_type(TY_STRUCT, 0, alignof(struct {})); }

/* --------------------------------------------------------------------------
   Integer promotions and usual arithmetic conversions (C11 6.3.1.8)
   -------------------------------------------------------------------------- */

/* Return the integer rank of a type. Higher rank means larger.
   Only call this for integer types (including enum). */
static int type_rank(Type *ty) {
  switch (ty->kind) {
  case TY_BOOL:
    return 1;
  case TY_CHAR:
    return 2;
  case TY_SHORT:
    return 3;
  case TY_INT:
  case TY_ENUM:
    return 4;
  case TY_LONG:
    return 5;
  default:
    unreachable();
    return 0;
  }
}

static Type *type_integer_promotion(Type *ty) {
  if (ty->kind == TY_ENUM)
    return ty_int;
  if (is_integer(ty) && ty->size < sizeof(int)) {
    return ty_int;
  }
  return ty;
}

static Type *type_usual_arithmetic_conversion(Type *t1, Type *t2) {

  // If there is a pointer in binary operation, we can return t1 type with
  // pointer. That's because in the new_add/new_dec function the pointer-related
  // operation has been expanded. E.g. (int*)ptr + 4 --> (int*)ptr + 4 *
  // (long)sizeof(int) then we can ignore the t2 type - we don't need this type
  // infomation to defer value so we can return t1 type with pointer if pointer
  // exists in this binary operation.
  // also, it could be used in compare operation.
  // It just like that: oh, we've expand any operation which including pointer
  // so now we can just type cast those expanded value to pointer.
  if (t1->base) {
    return pointer_to(t1->base);
  }

  if (t1->kind == TY_FUNC)
    return pointer_to(t1);
  if (t2->kind == TY_FUNC)
    return pointer_to(t2);
  t1 = type_integer_promotion(t1);
  t2 = type_integer_promotion(t2);

  if (t1 == t2)
    return t1;

  //  If either operand is floating-point
  if (is_flonum(t1) || is_flonum(t2)) {
    if (t1->kind == TY_LDOUBLE || t2->kind == TY_LDOUBLE)
      return ty_ldouble;
    if (t1->kind == TY_DOUBLE || t2->kind == TY_DOUBLE)
      return ty_double;
    return ty_float;
  }

  // Both operands are now integers
  int r1 = type_rank(t1), r2 = type_rank(t2);
  bool u1 = t1->is_unsigned, u2 = t2->is_unsigned;

  if (r1 > r2) {
    /* t1 has higher rank */
    if (u1)
      return t1; /* if t1 is unsigned, use it */
    /* t1 is signed. If t2 is unsigned and t1 can represent all values
       of t2, pick t1; otherwise pick the unsigned version of t1. */
    if (u2 && t1->size > t2->size)
      return t1; /* signed large enough */
    else if (u2)
      return (t1->kind == TY_LONG) ? ty_ulong : ty_uint;
    else
      return t1;
  } else if (r2 > r1) {
    /* symmetric case */
    if (u2)
      return t2;
    if (u1 && t2->size > t1->size)
      return t2;
    else if (u1)
      return (t2->kind == TY_LONG) ? ty_ulong : ty_uint;
    else
      return t2;
  } else {
    /* Same rank – if signedness differs, pick unsigned */
    if (u1 == u2)
      return t1;
    if (u2)
      return t2;
    return t1;
  }
}

static Node *integer_promotion(Node *n) {
  Type *promoted = type_integer_promotion(n->ty);
  if (promoted != n->ty)
    return new_cast(n, promoted);
  return n;
}

static Type *usual_arith_conv(Node **lhs, Node **rhs) {
  Type *ty = type_usual_arithmetic_conversion((*lhs)->ty, (*rhs)->ty);
  *lhs = new_cast(*lhs, ty);
  *rhs = new_cast(*rhs, ty);
  return ty;
}

static void integer_binary_operator_type_check(Node *lhs, Node *rhs) {
  if (!is_integer(rhs->ty) || !is_integer(lhs->ty)) {
    // is it possible to use their parent node to locate?
    error_tok((!is_integer(rhs->ty)) ? rhs->tok : lhs->tok,
              "invalid operands to this operand");
  }
}

void add_type(Node *node) {
  if (!node || node->ty) {
    return;
  }
  add_type(node->lhs);
  add_type(node->rhs);
  add_type(node->cond);
  add_type(node->then);
  add_type(node->_else);
  add_type(node->init);
  add_type(node->inc);

  for (Node *n = node->body; n; n = n->next) {
    add_type(n);
  }
  for (Node *n = node->args; n; n = n->next) {
    add_type(n);
  }

  switch (node->kind) {
  case ND_NUM:
    // I think num node should has the int type when created.
    // node->ty = ty_int;
    unreachable();
    return;
  // arithmetic binary operation
  // add operation has been canonicalized and pointer will always in the lhs
  case ND_ADD:
  // it's not allowed to set pointer in rhs in sub operation
  case ND_SUB:
  case ND_MUL:
  case ND_DIV: {
    node->ty = usual_arith_conv(&node->lhs, &node->rhs);
    return;
  }
  // integer-only binary operation
  case ND_MOD:
  case ND_BITAND:
  case ND_BITOR:
  case ND_BITXOR: {
    integer_binary_operator_type_check(node->lhs, node->rhs);
    node->ty = usual_arith_conv(&node->lhs, &node->rhs);
    return;
  }
  // integer-only unary operation
  case ND_BITNOT: {
    if (!is_integer(node->lhs->ty)) {
      error_tok(node->lhs->tok,
                "Invalid argument type (non-interger) for ~ operation");
    }
    node->lhs = integer_promotion(node->lhs);
    node->ty = node->lhs->ty;
    return;
  }
  // based on lhs type
  case ND_SHL:
  case ND_SHR: {
    integer_binary_operator_type_check(node->lhs, node->rhs);
    node->lhs = integer_promotion(node->lhs);
    node->rhs = integer_promotion(node->rhs);
    node->ty = node->lhs->ty;
    return;
  }
  // arithmetic unary operation
  case ND_NEG: {
    node->lhs = integer_promotion(node->lhs);
    node->ty = node->lhs->ty;
    return;
  }
  case ND_ASSIGN:
    if (node->lhs->ty->kind == TY_ARRAY)
      error_tok(node->lhs->tok, "not an lvalue");
    /* For struct assignment, we should eventually check type compatibility
       and insert memcpy. For now we leave the cast insertion only for
       non-struct types. */
    if (node->lhs->ty->kind != TY_STRUCT)
      node->rhs = new_cast(node->rhs, node->lhs->ty);
    node->ty = node->lhs->ty;
    return;
  // compare binary operation (always return int)
  case ND_EQ:
  case ND_NE:
  case ND_LT:
  case ND_LE: {
    usual_arith_conv(&node->lhs, &node->rhs);
    node->ty = ty_int;
    return;
  }
  case ND_FUNCALL:
    node->ty = node->func_ty->return_ty;
    return;
  // logic operation (always return int)
  case ND_NOT:
  case ND_LOGOR:
  case ND_LOGAND:
    node->ty = ty_int;
    return;
  case ND_VAR:
  case ND_VLA_PTR:
    node->ty = node->var->ty;
    return;
  case ND_COND:
    if (node->then->ty->kind == TY_VOID || node->_else->ty->kind == TY_VOID) {
      node->ty = ty_void;
    } else {
      node->ty = usual_arith_conv(&node->then, &node->_else);
    }
    return;
  case ND_COMMA:
    node->ty = node->rhs->ty;
    return;
  case ND_MEMBER:
    node->ty = node->member->ty;
    return;
  case ND_ADDR: {
    node->ty = pointer_to(node->lhs->ty);
    return;
  }
  case ND_DEREF:
    if (!node->lhs->ty->base)
      error_tok(node->tok, "invalid pointer dereference");
    if (node->lhs->ty->base->kind == TY_VOID)
      error_tok(node->tok, "dereferencing a void pointer");

    node->ty = node->lhs->ty->base;
    return;
  case ND_STMT_EXPR:
    if (node->body) {
      Node *stmt = node->body;
      while (stmt->next)
        stmt = stmt->next;
      if (stmt->kind == ND_EXPR_STMT) {
        node->ty = stmt->lhs->ty;
        return;
      }
    }
    error_tok(node->tok,
              "statement expression returning void is not supported");
    return;
  case ND_LABEL_VAL:
    node->ty = pointer_to(ty_void);
    return;
  case ND_CAS:
    add_type(node->cas_addr);
    add_type(node->cas_old);
    add_type(node->cas_new);
    node->ty = ty_bool;

    if (node->cas_addr->ty->kind != TY_PTR)
      error_tok(node->cas_addr->tok, "pointer expected");
    if (node->cas_old->ty->kind != TY_PTR)
      error_tok(node->cas_old->tok, "pointer expected");
    return;
  case ND_EXCH:
    if (node->lhs->ty->kind != TY_PTR)
      error_tok(node->cas_addr->tok, "pointer expected");
    node->ty = node->lhs->ty->base;
    return;
  default:
    break;
  }
}

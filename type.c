#include "dcc.h"
#include <stdalign.h>
#include <stdlib.h>

// basic type

Type *ty_void =
    &(Type){.kind = TY_VOID, .size = sizeof(void), .align = alignof(void)};
Type *ty_bool =
    &(Type){.kind = TY_BOOL, .size = sizeof(bool), .align = alignof(bool)};

Type *ty_char =
    &(Type){.kind = TY_CHAR, .size = sizeof(char), .align = alignof(char)};
Type *ty_short =
    &(Type){.kind = TY_SHORT, .size = sizeof(short), .align = alignof(short)};
Type *ty_int =
    &(Type){.kind = TY_INT, .size = sizeof(int), .align = alignof(int)};
Type *ty_long =
    &(Type){.kind = TY_LONG, .size = sizeof(long), .align = alignof(long)};

Type *ty_uchar = &(Type){.kind = TY_CHAR,
                         .size = sizeof(unsigned char),
                         .align = alignof(unsigned char),
                         .is_unsigned = true};
Type *ty_ushort = &(Type){.kind = TY_SHORT,
                          .size = sizeof(unsigned short),
                          .align = alignof(unsigned short),
                          .is_unsigned = true};
Type *ty_uint = &(Type){.kind = TY_INT,
                        .size = sizeof(unsigned int),
                        .align = alignof(unsigned int),
                        .is_unsigned = true};
Type *ty_ulong = &(Type){.kind = TY_LONG,
                         .size = sizeof(unsigned long),
                         .align = alignof(unsigned long),
                         .is_unsigned = true};

Type *ty_float =
    &(Type){.kind = TY_FLOAT, .size = sizeof(float), .align = alignof(float)};
Type *ty_double = &(Type){
    .kind = TY_DOUBLE, .size = sizeof(double), .align = alignof(double)};
Type *ty_ldouble = &(Type){.kind = TY_LDOUBLE,
                           .size = sizeof(long double),
                           .align = alignof(long double)};

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
    for (; p1 && p2; p1 = p1->next, p2 = p2->next)
      if (!is_compatible(p1, p2))
        return false;
    return p1 == NULL && p2 == NULL;
  }
  case TY_ARRAY: {
    if (!is_compatible(t1->base, t2->base))
      return false;
    return t1->array_len < 0 && t2->array_len < 0 &&
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
  // WARN: the function align in chibicc is 1, not 4
  Type *ty = new_type(TY_FUNC, sizeof(func_type), alignof(func_type));
  ty->return_ty = ret_ty;
  return ty;
}

Type *array_of(Type *base, int len) {
  Type *ty = new_type(TY_ARRAY, base->size * len, base->align);
  ty->base = base;
  ty->array_len = len;
  return ty;
}

// TODO: vla_of

Type *enum_type(void) {
  // if enum value is large enough, the size of enum may be bigger than int
  // but now we just ignore this feature and treat it as int
  return new_type(TY_ENUM, sizeof(int), sizeof(int));
}

// TODO: struct_type

// used to infering the common (largest) type in expression
static Type *get_common_type(Type *ty1, Type *ty2) {
  // cast array or pointer to pointer
  if (ty1->base)
    return pointer_to(ty1->base);
  // WARN: should I cast ty2 when it is pointer or array?
  // implict cast function to function pointer
  if (ty1->kind == TY_FUNC)
    return pointer_to(ty1);

  // upper cast
  if (ty1->kind == TY_LDOUBLE || ty2->kind == TY_LDOUBLE)
    return ty_ldouble;
  if (ty1->kind == TY_DOUBLE || ty2->kind == TY_DOUBLE)
    return ty_double;
  if (ty1->kind == TY_FLOAT || ty2->kind == TY_FLOAT)
    return ty_float;
  // any type small than int will be promoted to int
  if (ty1->size < sizeof(int))
    ty1 = ty_int;
  if (ty2->size < sizeof(int))
    ty2 = ty_int;

  if (ty1->size != ty2->size)
    return (ty1->size < ty2->size) ? ty2 : ty1;

  if (ty2->is_unsigned)
    return ty2;
  return ty1;
}
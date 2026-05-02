#ifndef DCC_H
#define DCC_H
#include <stdbool.h>
#include <stdint.h>
#include <stdnoreturn.h>

// tokenizer.c

typedef struct {
  char *name;
  char *contents;
} DFile;

typedef enum {
  TK_IDENT,   // Identifiers
  TK_PUNCT,   // Punctuators
  TK_KEYWORD, // Keywords
  TK_STR,     // String literals
  TK_NUM,     // Numeric literals
  TK_EOF,     // End-of-file markers
} TokenKind;

typedef struct Type Type;
typedef struct Token Token;
struct Token {
  TokenKind kind;
  char *loc; // token location
  int len;
  Type *ty; // TK_NUM or TK_STR
  char *str;
  uint64_t val;     // TK_NUM
  long double fval; // TK_NUM

  DFile *file; // Source location
  int line_no;
  Token *next;
};

noreturn void error(char *fmt, ...);

// type.c

typedef enum {
  TY_VOID,
  TY_BOOL,
  TY_CHAR,
  TY_SHORT,
  TY_INT,
  TY_LONG,
  TY_FLOAT,
  TY_DOUBLE,
  TY_LDOUBLE,
  TY_ENUM,
  TY_PTR,
  TY_FUNC,
  TY_ARRAY,
  TY_VLA, // variable-length array
  TY_STRUCT,
  TY_UNION,
} TypeKind;

struct Type {
  TypeKind kind;
  int size;         // sizeof()
  int align;        // alignment
  bool is_unsigned; // unsigned or signed for integer type
  bool is_atomic;   // TODO: _Atomic
  Type *origin;     // type compatibility check

  // chibicc:
  // Pointer-to or array-of type. We intentionally use the same member
  // to represent pointer/array duality in C.
  //
  // In many contexts in which a pointer is expected, we examine this
  // member instead of "kind" member to determine whether a type is a
  // pointer or not. That means in many contexts "array of T" is
  // naturally handled as if it were "pointer to T", as required by
  // the C spec.
  Type *base;

  // declaration
  Token *name;
  Token *name_pos;
  // array
  // array_len < 0 means array size inference
  int array_len;
  // TODO: vla
  // Node * vla_len;
  // Obj * vla_size;

  // TODO: Struct
  // Member *members;
  // bool is_flexible;
  // bool is_packed;

  // Function type
  Type *return_ty;
  Type *params;
  bool is_variadic;

  Type *next;
};

extern Type *ty_void;
extern Type *ty_bool;

extern Type *ty_char;
extern Type *ty_short;
extern Type *ty_int;
extern Type *ty_long;

extern Type *ty_uchar;
extern Type *ty_ushort;
extern Type *ty_uint;
extern Type *ty_ulong;

extern Type *ty_float;
extern Type *ty_double;
extern Type *ty_ldouble;

bool is_integer(Type *ty);
bool is_flonum(Type *ty);
bool is_numeric(Type *ty);
bool is_compatible(Type *t1, Type *t2);
Type *copy_type(Type *ty);
Type *pointer_to(Type *base);
Type *func_type(Type *return_ty);
Type *array_of(Type *base, int size);
// Type *vla_of(Type *base, Node *expr);
Type *enum_type(void);
// Type *struct_type(void);
// void add_type(Node *node);

#endif
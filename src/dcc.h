#ifndef DCC_H
#define DCC_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdnoreturn.h>
//
/// MISC
//

typedef struct Type Type;
typedef struct Token Token;
typedef struct Node Node;
typedef struct Obj Obj;
typedef struct Member Member;
typedef struct Initializer Initializer;

// Round up `n` to the nearest multiple of `align`. For instance,
// align_to(5, 8) returns 8 and align_to(11, 8) returns 16.
static inline int align_to(int n, int align) {
  return (n + align - 1) / align * align;
}

#define MAX(x, y) ((x) < (y) ? (y) : (x))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define next_iter_count(p)                                                     \
  ({                                                                           \
    size_t number = 0;                                                         \
    for (typeof(p) n = p; n; n = n->next)                                      \
      number++;                                                                \
    number;                                                                    \
  })
//
/// tokenizer.c
//

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

bool equal(Token *tok, char *op);
bool consume(Token **rest, Token *tok, char *str);
Token *skip(Token *tok, char *op);

noreturn void error(char *fmt, ...);
noreturn void error_tok(Token *tok, char *fmt, ...);
noreturn void error_at(char *loc, char *fmt, ...);
void warn_tok(Token *tok, char *fmt, ...);

Token *tokenize_string_literal(Token *tok, Type *basety);
Token *tokenize_file(char *path);
DFile *get_current_file();

#ifdef unreachable
#undef unreachable
#endif
#define unreachable() error("internal error at %s:%d", __FILE__, __LINE__)
#define todo_impl(feat)                                                        \
  error("feature [%s] is waiting for being implemented at %s:%d", feat,        \
        __FILE__, __LINE__)
//
/// type.c
//

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
  bool is_atomic;   // _Atomic
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
  // vla
  Node *vla_len;
  Obj *vla_size;

  // Struct
  Member *members;
  bool is_flexible;
  bool is_packed;

  // Function type
  Type *return_ty;
  Type *params;
  bool is_variadic;

  Type *next;
};

struct Member {
  Member *next;
  Type *ty;
  Token *tok; // for error message
  Token *name;
  int idx;
  int align;
  int offset;

  // Bitfield
  bool is_bitfield;
  int bit_offset;
  int bit_width;
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
bool is_agg_type(Type *ty);
bool is_large_agg_type(Type *ty);
bool is_compatible(Type *t1, Type *t2);
Type *copy_type(Type *ty);
Type *pointer_to(Type *base);
Type *func_type(Type *return_ty);
Type *array_of(Type *base, int size);
Type *vla_of(Type *base, Node *expr);
Type *enum_type(void);
Type *struct_type(void);
Type *struct_full_type(size_t nmem, Type **members);
Type *struct_full_type2(size_t nmem, Type **members, int *members_attr_align);
void add_type(Node *node, bool supress_decay);
Type *type_decay(Type *ty);

//
/// parser.c
//

typedef enum {
  ND_NULL_EXPR,  // Do nothing
  ND_NEG,        // unary -
  ND_ADD,        // +
  ND_SUB,        // -
  ND_MUL,        // *
  ND_DIV,        // /
  ND_MOD,        // %
  ND_BITAND,     // &
  ND_BITOR,      // |
  ND_BITXOR,     // ^
  ND_SHL,        // <<
  ND_SHR,        // >>
  ND_EQ,         // ==
  ND_NE,         // !=
  ND_LT,         // <
  ND_LE,         // <=
  ND_ASSIGN,     // =
  ND_COND,       // ?:
  ND_COMMA,      // ,
  ND_MEMBER,     // . (struct member access)
  ND_ADDR,       // unary &
  ND_DEREF,      // unary *
  ND_NOT,        // !
  ND_BITNOT,     // ~
  ND_LOGAND,     // &&
  ND_LOGOR,      // ||
  ND_RETURN,     // "return"
  ND_IF,         // "if"
  ND_FOR,        // "for" or "while"
  ND_DO,         // "do"
  ND_SWITCH,     // "switch"
  ND_CASE,       // "case"
  ND_BLOCK,      // { ... }
  ND_GOTO,       // "goto"
  ND_GOTO_EXPR,  // [GNU] "goto" labels-as-values
  ND_LABEL,      // Labeled statement
  ND_LABEL_VAL,  // [GNU] Labels-as-values
  ND_FUNCALL,    // Function call
  ND_EXPR_STMT,  // Expression statement
  ND_STMT_EXPR,  // [GNU] Statement expression
  ND_VAR,        // Variable
  ND_VLA_PTR,    //  VLA designator
  ND_NUM,        // Integer or fp
  ND_CAST,       // Type cast
  ND_MEMZERO,    // Zero-clear a stack variable
  ND_ASM,        // "asm"
  ND_CAS,        // Atomic compare-and-swap
  ND_EXCH,       // Atomic exchange
  ND_PTR_ADD,    // pointer operation(+)
  ND_PTR_SUB,    // pointer operation(-)
  ND_SA_ADD,     // +=
  ND_SA_PTR_ADD, // ptr +=
  ND_SA_SUB,     // -=
  ND_SA_PTR_SUB, // ptr -=
  ND_SA_MUL,     // *=
  ND_SA_DIV,     // /=
  ND_SA_MOD,     // %=
  ND_SA_BITAND,  // &=
  ND_SA_BITOR,   // |=
  ND_SA_BITXOR,  // ^=
  ND_SA_SHL,     // <<=
  ND_SA_SHR,     // >>=
  ND_ALLOCA,     // alloca
  ND_VA_START,   // va_start
  ND_VA_END,     // va_end
  ND_VA_COPY,    // va_copy
} NodeKind;

// AST node type
struct Node {
  NodeKind kind; // Node kind
  Node *next;    // Next node
  Type *ty; // Type, e.g. int or pointer to int or function calling return type
  Token *tok; // Representative token

  Node *lhs; // Left-hand side
  Node *rhs; // Right-hand side

  // "if" or "for" statement
  Node *cond;
  Node *then;
  Node *_else;
  Node *init;
  Node *inc;

  // "break" and "continue" labels
  char *break_label;
  char *cont_label;

  // Block or statement expression
  Node *body;

  // Struct member access
  Member *member;

  // Function call
  Type *func_ty;
  Node *args;
  bool pass_by_stack;
  Obj *ret_buffer;

  // Goto or labeled statement, or labels-as-values
  char *label;
  char *unique_label;
  Node *goto_next;

  // Switch
  Node *case_next;
  Node *default_case;

  // Case
  long begin;
  long end;

  // "asm" string literal
  char *asm_str;

  // Atomic compare-and-swap
  Node *cas_addr;
  Node *cas_old;
  Node *cas_new;

  // Atomic op= operators
  Obj *atomic_addr;
  Node *atomic_expr;

  // Variable
  Obj *var;

  // Numeric literal
  uint64_t val;
  long double fval;
};

struct Obj {
  Obj *next;
  char *name;    // Variable name
  Type *ty;      // Type
  Token *tok;    // representative token
  bool is_local; // local or global/function
  int align;     // alignment

  // Local variable

  // Could be used to store codegen data
  // e.g. offset for x86 asm backend
  // or LLVMValueRef(pointer) saved the reference to this obj for llvm backend
  intptr_t codegen_data;

  // Global variable or function
  bool is_function;
  bool is_definition;
  bool is_static;

  // Global variable

  // common linkage
  bool is_tentative;
  bool is_tls; // thread local
  Initializer *init;

  // Function
  bool is_inline;
  Obj *params; // positive order
  Node *body;
  // locals is special. the local variables are reversed order but the param
  // variables are positive ordered. Both of them are stored in this variable
  // [local var new] -- [local var old] [param old] -- [param new]
  Obj *locals;
};

// This struct represents a variable initializer. Since initializers
// can be nested (e.g. `int x[2][2] = {{1, 2}, {3, 4}}`), this struct
// is a tree data structure.
struct Initializer {
  Initializer *next;
  Type *ty;
  Token *tok;
  bool is_flexible;

  // If it's not an aggregate type and has an initializer,
  // `expr` has an initialization expression.
  Node *expr;

  // If it's an initializer for an aggregate type (e.g. array or struct),
  // `children` has initializers for its children.
  Initializer **children;

  // Only one member can be initialized for a union.
  // `mem` is used to clarify which member is initialized.
  Member *mem;
};

Node *new_cast(Node *expr, Type *ty);
int64_t const_expr(Token **rest, Token *tok);
Obj *parse(Token *tok);
int64_t eval(Node *node);
int64_t eval2(Node *node, char ***label);
double eval_double(Node *node);

//
// codegen
//

void codegen(Obj *prog, FILE *out, bool gen_asm);

//
/// strings.c
//

typedef struct {
  char **data;
  int capacity;
  int len;
} StringArray;

char *format(char *fmt, ...) __attribute__((format(printf, 1, 2)));
char *visual_bytes(char *s, size_t len);
void strarray_push(StringArray *arr, char *s);
void strarray_push_batch(StringArray *arr, char **append_array);
void strarray_push_batch2(StringArray *arr, StringArray *append_array);

//
/// hashmap.c
//

typedef struct {
  char *key;
  int keylen;
  void *val;
} HashEntry;

typedef struct {
  HashEntry *buckets;
  int capacity;
  int used;
} HashMap;

void *hashmap_get(HashMap *map, char *key);
void *hashmap_get2(HashMap *map, char *key, int keylen);
void hashmap_put(HashMap *map, char *key, void *val);
void hashmap_put2(HashMap *map, char *key, int keylen, void *val);
void hashmap_delete(HashMap *map, char *key);
void hashmap_delete2(HashMap *map, char *key, int keylen);
void hashmap_destroy(HashMap *map);
void hashmap_clear(HashMap *map);
bool hashmap_entry_valid(HashEntry *e);

#define hashmap_foreach(map, entry)                                            \
  for (int __hashmap_i = 0; __hashmap_i < (map)->capacity; __hashmap_i++)      \
    if (hashmap_entry_valid((map)->buckets + __hashmap_i))                     \
      for (HashEntry *entry = (map)->buckets + __hashmap_i, *__once = NULL;    \
           !__once; __once = (void *)1)

//
/// unicode.c
//

bool is_ident2(uint32_t c);
bool is_ident1(uint32_t c);
int utf8_encode(char *buf, uint32_t code_point);
uint32_t utf8_decode(char **new_pos, char *p);
int display_width(char *p, int len);

//
/// args.c
//

void parse_args(int argc, char **argv);
extern bool opt_cc1;
extern bool opt_S;
extern bool opt_c;
extern bool opt_E;
extern bool opt_hash_hash_hash;
extern bool opt_ir;
extern bool opt_static;
extern bool opt_shared;
extern bool opt_fcommon;
extern bool opt_fpic; // unused

extern char *opt_cc1_output;
extern char *opt_cc1_input;
extern char *opt_o;

extern StringArray opt_input_paths;
extern StringArray opt_ld_extra_args;
extern StringArray opt_cpp_extra_args;

extern bool opt_M;
extern bool opt_MD;
extern bool opt_MM;
extern bool opt_MMD;
extern bool opt_MP;
extern bool opt_MG;
extern char *opt_MF;
extern char *opt_MT;

//
/// pathlib.c
//

char *path_new_tmpfile(void);
void path_fcp(FILE *dst, FILE *src);
void path_cp(char *dst, char *src);
char *path_new_replaced_suffix(char *path, char *suffix);
char *path_find_file(char *pattern);
bool path_exists(char *path);

#endif
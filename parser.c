#include "dcc.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Scope for local variables, global variables, typedefs
// or enum constants
typedef struct {
  Obj *var;
  Type *type_def;
  Type *enum_ty;
  int enum_val;
} VarScope;

// Represents a block scope.
typedef struct Scope Scope;
struct Scope {
  Scope *next;

  // C has two block scopes; one is for variables/typedefs and
  // the other is for struct/union/enum tags.
  HashMap vars;
  HashMap tags;
};

// Variable attributes such as typedef or extern.
typedef struct {
  bool is_typedef;
  bool is_static;
  bool is_extern;
  bool is_inline;
  bool is_tls;
  int align;
} VarAttr;

// All local variable instances created during parsing are
// accumulated to this list.
static Obj *locals;

// Likewise, global variables are accumulated to this list.
static Obj *globals;

static Scope *scope = &(Scope){};

// Points to the function object the parser is currently parsing.
static Obj *current_fn;

// Lists of all goto statements and labels in the curent function.
static Node *gotos;
static Node *labels;

// Current "goto" and "continue" jump targets.
static char *brk_label;
static char *cont_label;

// Points to a node representing a switch if we are parsing
// a switch statement. Otherwise, NULL.
static Node *current_switch;

static Obj *builtin_alloca;

static bool is_typename(Token *tok);
static Type *declspec(Token **rest, Token *tok, VarAttr *attr);
static Type *typename(Token **rest, Token *tok);
static Type *enum_specifier(Token **rest, Token *tok);
static Type *typeof_specifier(Token **rest, Token *tok);
static Type *type_suffix(Token **rest, Token *tok, Type *ty);
static Type *declarator(Token **rest, Token *tok, Type *ty);
static Node *declaration(Token **rest, Token *tok, Type *basety, VarAttr *attr);
// static void array_initializer2(Token **rest, Token *tok, Initializer *init,
//                                int i);
// static void struct_initializer2(Token **rest, Token *tok, Initializer *init,
//                                 Member *mem);
// static void initializer2(Token **rest, Token *tok, Initializer *init);
// static Initializer *initializer(Token **rest, Token *tok, Type *ty,
//                                 Type **new_ty);
static Node *lvar_initializer(Token **rest, Token *tok, Obj *var);
static void gvar_initializer(Token **rest, Token *tok, Obj *var);
static Node *compound_stmt(Token **rest, Token *tok);
static Node *stmt(Token **rest, Token *tok);
static Node *expr_stmt(Token **rest, Token *tok);
static Node *expr(Token **rest, Token *tok);
static int64_t eval(Node *node);
static int64_t eval2(Node *node, char ***label);
static int64_t eval_rval(Node *node, char ***label);
static bool is_const_expr(Node *node);
static Node *assign(Token **rest, Token *tok);
static Node *logor(Token **rest, Token *tok);
static double eval_double(Node *node);
static Node *conditional(Token **rest, Token *tok);
static Node *logand(Token **rest, Token *tok);
static Node *bitor(Token **rest, Token *tok);
static Node *bitxor(Token **rest, Token *tok);
static Node *bitand(Token **rest, Token *tok);
static Node *equality(Token **rest, Token *tok);
static Node *relational(Token **rest, Token *tok);
static Node *shift(Token **rest, Token *tok);
static Node *add(Token **rest, Token *tok);
static Node *new_add(Node *lhs, Node *rhs, Token *tok);
static Node *new_sub(Node *lhs, Node *rhs, Token *tok);
static Node *mul(Token **rest, Token *tok);
static Node *cast(Token **rest, Token *tok);
static Member *get_struct_member(Type *ty, Token *tok);
static Type *struct_decl(Token **rest, Token *tok);
static Type *union_decl(Token **rest, Token *tok);
static Node *postfix(Token **rest, Token *tok);
static Node *funcall(Token **rest, Token *tok, Node *node);
static Node *unary(Token **rest, Token *tok);
static Node *primary(Token **rest, Token *tok);
static Token *parse_typedef(Token *tok, Type *basety);
static bool is_function(Token *tok);
static Token *function(Token *tok, Type *basety, VarAttr *attr);
static Token *global_variable(Token *tok, Type *basety, VarAttr *attr);

static int align_down(int n, int align) {
  return align_to(n - align + 1, align);
}

static void enter_scope(void) {
  Scope *sc = calloc(1, sizeof(Scope));
  sc->next = scope;
  scope = sc;
}

static void leave_scope(void) { scope = scope->next; }

// Find a variable by name.
static VarScope *find_var(Token *tok) {
  for (Scope *sc = scope; sc; sc = sc->next) {
    VarScope *sc2 = hashmap_get2(&sc->vars, tok->loc, tok->len);
    if (sc2)
      return sc2;
  }
  return NULL;
}

static Type *find_tag(Token *tok) {
  for (Scope *sc = scope; sc; sc = sc->next) {
    Type *ty = hashmap_get2(&sc->tags, tok->loc, tok->len);
    if (ty)
      return ty;
  }
  return NULL;
}

static Node *new_node(NodeKind kind, Token *tok) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->tok = tok;
  return node;
}

// used to represent some basic int type constant value
static Node *new_num(int val, Token *tok) {
  Node *node = new_node(ND_NUM, tok);
  node->val = (uint64_t)val;
  node->ty = ty_int;
  return node;
}

static Node *new_long(int64_t val, Token *tok) {
  Node *node = new_node(ND_NUM, tok);
  node->val = val;
  node->ty = ty_long;
  return node;
}

// used to represent some ulong type constant value
static Node *new_ulong(unsigned long val, Token *tok) {
  Node *node = new_node(ND_NUM, tok);
  node->val = val;
  node->ty = ty_ulong;
  return node;
}

static Node *new_var_node(Obj *var, Token *tok) {
  Node *node = new_node(ND_VAR, tok);
  node->var = var;
  return node;
}

static VarScope *push_scope(char *name) {
  VarScope *sc = calloc(1, sizeof(VarScope));
  hashmap_put(&scope->vars, name, sc);
  return sc;
}

static Node *new_vla_ptr(Obj *var, Token *tok) {
  Node *node = new_node(ND_VLA_PTR, tok);
  node->var = var;
  return node;
}

static Obj *new_var(char *name, Type *ty) {
  Obj *var = calloc(1, sizeof(Obj));
  var->name = name;
  var->ty = ty;
  var->align = ty->align;
  push_scope(name)->var = var;
  return var;
}

static Obj *new_lvar(char *name, Type *ty) {
  Obj *var = new_var(name, ty);
  var->is_local = true;
  var->next = locals;
  locals = var;
  return var;
}

static Obj *new_gvar(char *name, Type *ty) {
  Obj *var = new_var(name, ty);
  var->next = globals;
  var->is_static = true;
  var->is_definition = true;
  globals = var;
  return var;
}

static char *new_unique_name(void) {
  static int id = 0;
  return format(".L..%d", id++);
}

static Obj *new_anon_gvar(Type *ty) { return new_gvar(new_unique_name(), ty); }

static Obj *new_string_literal(char *p, Type *ty) {
  Obj *var = new_anon_gvar(ty);
  var->init_data = p;
  return var;
}

static char *get_ident(Token *tok) {
  if (tok->kind != TK_IDENT)
    error_tok(tok, "expected an identifier");
  return strndup(tok->loc, tok->len);
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

static Type *find_typedef(Token *tok) {
  if (tok->kind == TK_IDENT) {
    VarScope *sc = find_var(tok);
    if (sc)
      return sc->type_def;
  }
  return NULL;
}

static void push_tag_scope(Token *tok, Type *ty) {
  hashmap_put2(&scope->tags, tok->loc, tok->len, ty);
}

// primary = "(" "{" stmt+ "}" ")"
//         | "(" expr ")"
//         | "sizeof" "(" type-name ")"
//         | "sizeof" unary
//         | "_Alignof" "(" type-name ")"
//         | "_Alignof" unary
//         | "_Generic" generic-selection
//         | "__builtin_types_compatible_p" "(" type-name, type-name, ")"
//         | "__builtin_reg_class" "(" type-name ")" // [chibicc] used in va_arg
//         | ident
//         | str
//         | num
static Node *primary(Token **rest, Token *tok) {
  Token *start = tok;
  // "(" "{" stmt+ "}" ")"
  if (equal(tok, "(") && equal(tok->next, "{")) {
    // [GNU] statement expression
    Node *node = new_node(ND_STMT_EXPR, tok);
    node->body = compound_stmt(&tok, tok->next->next)->body;
    *rest = skip(tok, ")");
    return node;
  }
  // "(" expr ")"
  if (equal(tok, "(")) {
    Node *node = expr(&tok, tok->next);
    *rest = skip(tok, ")");
    return node;
  }
  // "sizeof" "(" type-name ")"
  if (equal(tok, "sizeof") && equal(tok->next, "(") &&
      is_typename(tok->next->next)) {
    Type *ty = typename(&tok, tok->next->next);
    *rest = skip(tok, ")");

    if (ty->kind == TY_VLA) {
      // TODO:
      unreachable();
      // if (ty->vla_size)
      //   return new_var_node(ty->vla_size, tok);

      // Node *lhs = compute_vla_size(ty, tok);
      // Node *rhs = new_var_node(ty->vla_size, tok);
      // return new_binary(ND_COMMA, lhs, rhs, tok);
    }

    return new_ulong(ty->size, start);
  }
  // "sizeof" unary
  if (equal(tok, "sizeof")) {
    Node *node = unary(rest, tok->next);
    add_type(node);
    if (node->ty->kind == TY_VLA)
      return new_var_node(node->ty->vla_size, tok);
    return new_ulong(node->ty->size, tok);
  }
  // "_Alignof" "(" type-name ")"
  if (equal(tok, "_Alignof") && equal(tok->next, "(") &&
      is_typename(tok->next->next)) {
    Type *ty = typename(&tok, tok->next->next);
    *rest = skip(tok, ")");
    return new_ulong(ty->align, tok);
  }
  //  "_Alignof" unary
  if (equal(tok, "_Alignof")) {
    Node *node = unary(rest, tok->next);
    add_type(node);
    return new_ulong(node->ty->align, tok);
  }
  // "_Generic" generic-selection
  if (equal(tok, "_Generic")) {
    // TODO:
    unreachable();
    // return generic_selection(rest, tok->next);
  }

  if (equal(tok, "__builtin_types_compatible_p")) {
    tok = skip(tok->next, "(");
    Type *t1 = typename(&tok, tok);
    tok = skip(tok, ",");
    Type *t2 = typename(&tok, tok);
    *rest = skip(tok, ")");
    return new_num(is_compatible(t1, t2), start);
  }

  if (equal(tok, "__builtin_reg_class")) {
    // TODO:
    unreachable();
    tok = skip(tok->next, "(");
    Type *ty = typename(&tok, tok);
    *rest = skip(tok, ")");

    if (is_integer(ty) || ty->kind == TY_PTR)
      return new_num(0, start);
    if (is_flonum(ty))
      return new_num(1, start);
    return new_num(2, start);
  }

  if (equal(tok, "__builtin_compare_and_swap")) {
    Node *node = new_node(ND_CAS, tok);
    tok = skip(tok->next, "(");
    node->cas_addr = assign(&tok, tok);
    tok = skip(tok, ",");
    node->cas_old = assign(&tok, tok);
    tok = skip(tok, ",");
    node->cas_new = assign(&tok, tok);
    *rest = skip(tok, ")");
    return node;
  }

  if (equal(tok, "__builtin_atomic_exchange")) {
    Node *node = new_node(ND_EXCH, tok);
    tok = skip(tok->next, "(");
    node->lhs = assign(&tok, tok);
    tok = skip(tok, ",");
    node->rhs = assign(&tok, tok);
    *rest = skip(tok, ")");
    return node;
  }

  if (tok->kind == TK_IDENT) {
    // Variable or enum constant
    VarScope *sc = find_var(tok);
    *rest = tok->next;

    // For "static inline" function
    if (sc && sc->var && sc->var->is_function) {
      unreachable();
      // if (current_fn)
      //   strarray_push(&current_fn->refs, sc->var->name);
      // else
      //   sc->var->is_root = true;
    }

    if (sc) {
      if (sc->var)
        return new_var_node(sc->var, tok);
      if (sc->enum_ty)
        return new_num(sc->enum_val, tok);
    }

    if (equal(tok->next, "("))
      error_tok(tok, "implicit declaration of a function");
    error_tok(tok, "undefined variable");
  }

  if (tok->kind == TK_STR) {
    Obj *var = new_string_literal(tok->str, tok->ty);
    *rest = tok->next;
    return new_var_node(var, tok);
  }

  if (tok->kind == TK_NUM) {
    Node *node = new_node(ND_NUM, tok);
    node->ty = tok->ty;
    if (is_flonum(tok->ty)) {
      node->fval = tok->fval;
    } else {
      node->val = tok->val;
    }
    *rest = tok->next;
    return node;
  }

  error_tok(tok, "expected a primary expression");
}

static void declare_builtin_functions(void) {
  Type *ty = func_type(pointer_to(ty_void));
  ty->params = copy_type(ty_int);
  builtin_alloca = new_gvar("alloca", ty);
  builtin_alloca->is_definition = false;
}

Obj *parse(Token *tok) {
  declare_builtin_functions();
  globals = NULL;

  while (tok->kind != TK_EOF) {
    VarAttr attr = {};
    Type *basety = declspec(&tok, tok, &attr);

    if (attr.is_typedef) {
      tok = parse_typedef(tok, basety);
      continue;
    }

    if (is_function(tok)) {
      tok = function(tok, basety, &attr);
      continue;
    }

    tok = global_variable(tok, basety, &attr);
  }
  // TODO: static inline related
  // for (Obj *var = globals; var; var = var->next)
  //   if (var->is_root)
  //     mark_live(var);

  // scan_globals();
  return globals;
}
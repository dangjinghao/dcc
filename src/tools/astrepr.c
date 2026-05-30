#include "dcc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define array_string_item(X) [X] = #X

static char *NodeKind_string[] = {
    array_string_item(ND_NULL_EXPR), array_string_item(ND_ADD),
    array_string_item(ND_SUB),       array_string_item(ND_MUL),
    array_string_item(ND_DIV),       array_string_item(ND_NEG),
    array_string_item(ND_MOD),       array_string_item(ND_BITAND),
    array_string_item(ND_BITOR),     array_string_item(ND_BITXOR),
    array_string_item(ND_SHL),       array_string_item(ND_SHR),
    array_string_item(ND_EQ),        array_string_item(ND_NE),
    array_string_item(ND_LT),        array_string_item(ND_LE),
    array_string_item(ND_ASSIGN),    array_string_item(ND_COND),
    array_string_item(ND_COMMA),     array_string_item(ND_MEMBER),
    array_string_item(ND_ADDR),      array_string_item(ND_DEREF),
    array_string_item(ND_NOT),       array_string_item(ND_BITNOT),
    array_string_item(ND_LOGAND),    array_string_item(ND_LOGOR),
    array_string_item(ND_RETURN),    array_string_item(ND_IF),
    array_string_item(ND_FOR),       array_string_item(ND_DO),
    array_string_item(ND_SWITCH),    array_string_item(ND_CASE),
    array_string_item(ND_BLOCK),     array_string_item(ND_GOTO),
    array_string_item(ND_GOTO_EXPR), array_string_item(ND_LABEL),
    array_string_item(ND_LABEL_VAL), array_string_item(ND_FUNCALL),
    array_string_item(ND_EXPR_STMT), array_string_item(ND_STMT_EXPR),
    array_string_item(ND_VAR),       array_string_item(ND_VLA_PTR),
    array_string_item(ND_NUM),       array_string_item(ND_CAST),
    array_string_item(ND_MEMZERO),   array_string_item(ND_ASM),
    array_string_item(ND_CAS),       array_string_item(ND_EXCH),
    array_string_item(ND_SA_ADD),    array_string_item(ND_SA_SUB),
    array_string_item(ND_SA_MUL),    array_string_item(ND_SA_DIV),
    array_string_item(ND_SA_MOD),    array_string_item(ND_SA_BITAND),
    array_string_item(ND_SA_BITOR),  array_string_item(ND_SA_BITXOR),
    array_string_item(ND_SA_SHL),    array_string_item(ND_SA_SHR),
    array_string_item(ND_ALLOCA),    array_string_item(ND_VA_START),
    array_string_item(ND_VA_END),    array_string_item(ND_VA_COPY),
    array_string_item(ND_POST_DEC),  array_string_item(ND_POST_INC),
};

static char *TypeKind_string[] = {
    array_string_item(TY_VOID),    array_string_item(TY_BOOL),
    array_string_item(TY_CHAR),    array_string_item(TY_SHORT),
    array_string_item(TY_INT),     array_string_item(TY_LONG),
    array_string_item(TY_FLOAT),   array_string_item(TY_DOUBLE),
    array_string_item(TY_LDOUBLE), array_string_item(TY_ENUM),
    array_string_item(TY_PTR),     array_string_item(TY_FUNC),
    array_string_item(TY_ARRAY),   array_string_item(TY_VLA),
    array_string_item(TY_STRUCT),  array_string_item(TY_UNION),
};

#undef array_string_item

typedef struct {
  FILE *out;
  HashMap seen_obj;
  HashMap seen_node;
  HashMap seen_type;
  HashMap seen_member;
  HashMap seen_reloc;
} ReprCtx;

typedef struct {
  FILE *out;
  bool started;
  bool first;
} ChildList;

static void childlist_init(ChildList *cl, FILE *out) {
  cl->out = out;
  cl->started = false;
  cl->first = true;
}

static void childlist_add_start(ChildList *cl) {
  if (!cl->started) {
    fprintf(cl->out, ",\"children\":[");
    cl->started = true;
  }
  if (!cl->first)
    fputc(',', cl->out);
  cl->first = false;
}

static void childlist_close(ChildList *cl) {
  if (cl->started)
    fputc(']', cl->out);
}

static void json_write_string(FILE *out, const char *s) {
  fputc('"', out);
  if (!s) {
    fputc('"', out);
    return;
  }
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    unsigned char c = *p;
    switch (c) {
    case '\\':
      fputs("\\\\", out);
      break;
    case '"':
      fputs("\\\"", out);
      break;
    case '\n':
      fputs("\\n", out);
      break;
    case '\r':
      fputs("\\r", out);
      break;
    case '\t':
      fputs("\\t", out);
      break;
    default:
      if (c < 0x20) {
        fprintf(out, "\\u%04x", c);
      } else {
        fputc(c, out);
      }
    }
  }
  fputc('"', out);
}

static bool seen_put(HashMap *map, void *ptr) {
  char *key = format("%p", ptr);
  if (hashmap_get(map, key))
    return true;
  hashmap_put(map, key, (void *)1);
  return false;
}

static void write_obj(ReprCtx *ctx, Obj *o);
static void write_node(ReprCtx *ctx, Node *n);
static void write_type(ReprCtx *ctx, Type *ty);
static void write_member(ReprCtx *ctx, Member *mem);

static void write_node_list(ReprCtx *ctx, ChildList *cl, Node *n) {
  for (Node *cur = n; cur; cur = cur->next) {
    childlist_add_start(cl);
    write_node(ctx, cur);
  }
}

static void write_type_list(ReprCtx *ctx, ChildList *cl, Type *ty) {
  for (Type *cur = ty; cur; cur = cur->next) {
    childlist_add_start(cl);
    write_type(ctx, cur);
  }
}

static void write_obj_list(ReprCtx *ctx, ChildList *cl, Obj *o) {
  for (Obj *cur = o; cur; cur = cur->next) {
    childlist_add_start(cl);
    write_obj(ctx, cur);
  }
}

static void write_member_list(ReprCtx *ctx, ChildList *cl, Member *mem) {
  for (Member *cur = mem; cur; cur = cur->next) {
    childlist_add_start(cl);
    write_member(ctx, cur);
  }
}

static void write_ref(FILE *out, const char *summary) {
  fprintf(out, "{\"name\":");
  json_write_string(out, summary);
  fprintf(out, "}");
}

static void write_obj(ReprCtx *ctx, Obj *o) {
  if (!o) {
    fprintf(ctx->out, "{\"name\":\"Obj(null)\"}");
    return;
  }
  if (seen_put(&ctx->seen_obj, o)) {
    char *summary =
        format("Obj(ref) %s%s%s", o->name ? o->name : "(anon)",
               o->is_function ? " (func)" : "", o->is_local ? " (local)" : "");
    write_ref(ctx->out, summary);
    return;
  }

  char *name =
      format("Obj %s%s%s", o->name ? o->name : "(anon)",
             o->is_function ? " (func)" : "", o->is_local ? " (local)" : "");

  fprintf(ctx->out, "{\"name\":");
  json_write_string(ctx->out, name);

  ChildList cl;
  childlist_init(&cl, ctx->out);

  if (o->ty) {
    childlist_add_start(&cl);
    write_type(ctx, o->ty);
  }

  if (o->params) {
    childlist_add_start(&cl);
    fprintf(ctx->out, "{\"name\":\"params\"");
    ChildList pl;
    childlist_init(&pl, ctx->out);
    write_obj_list(ctx, &pl, o->params);
    childlist_close(&pl);
    fprintf(ctx->out, "}");
  }

  if (o->locals) {
    childlist_add_start(&cl);
    fprintf(ctx->out, "{\"name\":\"locals\"");
    ChildList ll;
    childlist_init(&ll, ctx->out);
    write_obj_list(ctx, &ll, o->locals);
    childlist_close(&ll);
    fprintf(ctx->out, "}");
  }

  if (o->body) {
    childlist_add_start(&cl);
    fprintf(ctx->out, "{\"name\":\"body\"");
    ChildList bl;
    childlist_init(&bl, ctx->out);
    childlist_add_start(&bl);
    write_node(ctx, o->body);
    childlist_close(&bl);
    fprintf(ctx->out, "}");
  }

  if (o->init) {
    childlist_add_start(&cl);
    char *idata = format("*initialized*");
    fprintf(ctx->out, "{\"name\":");
    json_write_string(ctx->out, idata);
    fprintf(ctx->out, "}");
  }

  childlist_close(&cl);
  fprintf(ctx->out, "}");
}

static void write_node(ReprCtx *ctx, Node *n) {
  if (!n) {
    fprintf(ctx->out, "{\"name\":\"Node(null)\"}");
    return;
  }
  if (seen_put(&ctx->seen_node, n)) {
    const char *kind = NodeKind_string[n->kind];
    char *summary = NULL;
    if (n->kind == ND_NUM) {
      summary = format("Node(ref) %s %llu", kind, (unsigned long long)n->val);
    } else if (n->kind == ND_VAR && n->var && n->var->name) {
      summary = format("Node(ref) %s %s", kind, n->var->name);
    } else if (n->kind == ND_LABEL && n->label) {
      summary = format("Node(ref) %s %s", kind, n->label);
    } else {
      summary = format("Node(ref) %s", kind);
    }
    write_ref(ctx->out, summary);
    return;
  }

  const char *kind = NodeKind_string[n->kind];
  char *name = NULL;
  if (n->kind == ND_NUM) {
    name = format("%s %llu", kind, (unsigned long long)n->val);
  } else if (n->kind == ND_VAR && n->var && n->var->name) {
    name = format("%s %s", kind, n->var->name);
  } else if (n->kind == ND_LABEL && n->label) {
    name = format("%s %s", kind, n->label);
  } else {
    name = format("%s", kind);
  }

  fprintf(ctx->out, "{\"name\":");
  json_write_string(ctx->out, name);

  ChildList cl;
  childlist_init(&cl, ctx->out);

  if (n->ty) {
    childlist_add_start(&cl);
    write_type(ctx, n->ty);
  }
  if (n->lhs) {
    childlist_add_start(&cl);
    write_node(ctx, n->lhs);
  }
  if (n->rhs) {
    childlist_add_start(&cl);
    write_node(ctx, n->rhs);
  }
  if (n->cond) {
    childlist_add_start(&cl);
    write_node(ctx, n->cond);
  }
  if (n->then) {
    childlist_add_start(&cl);
    write_node(ctx, n->then);
  }
  if (n->_else) {
    childlist_add_start(&cl);
    write_node(ctx, n->_else);
  }
  if (n->init) {
    childlist_add_start(&cl);
    write_node(ctx, n->init);
  }
  if (n->inc) {
    childlist_add_start(&cl);
    write_node(ctx, n->inc);
  }
  if (n->body) {
    write_node_list(ctx, &cl, n->body);
  }
  if (n->args) {
    write_node_list(ctx, &cl, n->args);
  }
  if (n->case_next) {
    write_node_list(ctx, &cl, n->case_next);
  }
  if (n->default_case) {
    childlist_add_start(&cl);
    write_node(ctx, n->default_case);
  }
  if (n->cas_addr || n->cas_old || n->cas_new) {
    childlist_add_start(&cl);
    fprintf(ctx->out, "{\"name\":\"cas\"");
    ChildList casl;
    childlist_init(&casl, ctx->out);
    if (n->cas_addr) {
      childlist_add_start(&casl);
      write_node(ctx, n->cas_addr);
    }
    if (n->cas_old) {
      childlist_add_start(&casl);
      write_node(ctx, n->cas_old);
    }
    if (n->cas_new) {
      childlist_add_start(&casl);
      write_node(ctx, n->cas_new);
    }
    childlist_close(&casl);
    fprintf(ctx->out, "}");
  }
  if (n->var) {
    childlist_add_start(&cl);
    write_obj(ctx, n->var);
  }
  if (n->member) {
    childlist_add_start(&cl);
    write_member(ctx, n->member);
  }

  childlist_close(&cl);
  fprintf(ctx->out, "}");
}

static void write_member(ReprCtx *ctx, Member *mem) {
  if (!mem) {
    fprintf(ctx->out, "{\"name\":\"Member(null)\"}");
    return;
  }
  if (seen_put(&ctx->seen_member, mem)) {
    char *summary =
        mem->name ? format("Member(ref) %.*s", mem->name->len, mem->name->loc)
                  : format("Member(ref) (anon)");
    write_ref(ctx->out, summary);
    return;
  }

  char *name = mem->name ? format("Member %.*s", mem->name->len, mem->name->loc)
                         : format("Member (anon)");
  fprintf(ctx->out, "{\"name\":");
  json_write_string(ctx->out, name);

  ChildList cl;
  childlist_init(&cl, ctx->out);

  if (mem->ty) {
    childlist_add_start(&cl);
    write_type(ctx, mem->ty);
  }

  childlist_close(&cl);
  fprintf(ctx->out, "}");
}

static void write_type(ReprCtx *ctx, Type *ty) {
  if (!ty) {
    fprintf(ctx->out, "{\"name\":\"Type(null)\"}");
    return;
  }
  if (seen_put(&ctx->seen_type, ty)) {
    const char *k = TypeKind_string[ty->kind];
    char *summary =
        format("Type(ref) %s size=%d align=%d", k, ty->size, ty->align);
    write_ref(ctx->out, summary);
    return;
  }

  const char *k = TypeKind_string[ty->kind];
  char *name = format("Type %s size=%d align=%d", k, ty->size, ty->align);

  fprintf(ctx->out, "{\"name\":");
  json_write_string(ctx->out, name);

  ChildList cl;
  childlist_init(&cl, ctx->out);

  if (ty->base) {
    childlist_add_start(&cl);
    write_type(ctx, ty->base);
  }
  if (ty->return_ty) {
    childlist_add_start(&cl);
    write_type(ctx, ty->return_ty);
  }
  if (ty->params) {
    childlist_add_start(&cl);
    fprintf(ctx->out, "{\"name\":\"params\"");
    ChildList pl;
    childlist_init(&pl, ctx->out);
    write_type_list(ctx, &pl, ty->params);
    childlist_close(&pl);
    fprintf(ctx->out, "}");
  }
  if (ty->members) {
    childlist_add_start(&cl);
    fprintf(ctx->out, "{\"name\":\"members\"");
    ChildList ml;
    childlist_init(&ml, ctx->out);
    write_member_list(ctx, &ml, ty->members);
    childlist_close(&ml);
    fprintf(ctx->out, "}");
  }
  if (ty->vla_len) {
    childlist_add_start(&cl);
    write_node(ctx, ty->vla_len);
  }
  if (ty->vla_size) {
    childlist_add_start(&cl);
    write_obj(ctx, ty->vla_size);
  }

  childlist_close(&cl);
  fprintf(ctx->out, "}");
}

char *objrepr(Obj *o) {
  ReprCtx ctx = {.out = NULL};
  char *buf = NULL;
  size_t size = 0;
  ctx.out = open_memstream(&buf, &size);
  if (!ctx.out)
    return NULL;

  fprintf(ctx.out, "{\"name\":\"ObjRoot\"");
  ChildList cl;
  childlist_init(&cl, ctx.out);
  write_obj_list(&ctx, &cl, o);
  childlist_close(&cl);
  fprintf(ctx.out, "}");

  fflush(ctx.out);
  fclose(ctx.out);
  return buf;
}

char *noderepr(Node *n) {
  ReprCtx ctx = {.out = NULL};
  char *buf = NULL;
  size_t size = 0;
  ctx.out = open_memstream(&buf, &size);
  if (!ctx.out)
    return NULL;

  write_node(&ctx, n);

  fflush(ctx.out);
  fclose(ctx.out);
  return buf;
}

char *typerepr(Type *ty) {
  ReprCtx ctx = {.out = NULL};
  char *buf = NULL;
  size_t size = 0;
  ctx.out = open_memstream(&buf, &size);
  if (!ctx.out)
    return NULL;

  write_type(&ctx, ty);

  fflush(ctx.out);
  fclose(ctx.out);
  return buf;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    exit(1);
  }
  Token *ts = tokenize_file(argv[1]);

  Obj *o = parse(ts);
  char *s = objrepr(o);
  puts(s);
  return 0;
}

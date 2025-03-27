#include "builder.h"
#include "log/log.h"
#include "parser.h"
#include "sds/sds.h"
#include "slist/slist.h"
#include <llvm-c/Core.h>
#include <llvm-c/Support.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Types.h>

const char *STATIC_VAR_FMT = "%s.%ld";
const char *STRUCT_FMT = "struct.%s.%ld";
const char *UNION_FMT = "union.%s.%ld";
const char *VAR_FMT = "%s.%ld";
const char *GOTO_BLK_FMT = "goto.%s";
const char *CASE_BLK_FMT = "case.%ld";
builder builder_new(builder b, char *module_name, slist symtab) {
  b->context = LLVMContextCreate();
  b->module = LLVMModuleCreateWithNameInContext(module_name, b->context);
  b->builder = LLVMCreateBuilderInContext(b->context);

  const char *targetTriple = LLVMGetDefaultTargetTriple();
  log_debug("target triple: %s", targetTriple);
  LLVMSetTarget(b->module, targetTriple);
  b->data_layout = LLVMGetModuleDataLayout(b->module);

  b->symtab = symtab;
  b->fn = NULL;

  slist_init(&b->labels);
  return b;
}

goto_label builder_label_find(builder b, sds name) {
  struct label *label;
  slist_foreach(&b->labels, label) {
    if (sdscmp(label->name, name) == 0) {
      return label;
    }
  }
  return NULL;
}

goto_label builder_label_new(builder b, sds name) {
  goto_label l = calloc(1, sizeof(struct label));
  sds block_name = sdscatfmt(sdsempty(), GOTO_BLK_FMT, name);
  l->name = sdsdup(name);
  l->block = LLVMAppendBasicBlockInContext(b->context, b->fn, block_name);
  l->defined = false;
  slist_add_tail(&b->labels, l);
  sdsfree(block_name);
  return l;
}

void builder_label_list_check_undefined(builder b) {
  struct label *label;
  slist_foreach(&b->labels, label) {
    if (!label->defined) {
      log_panic("label %s is undefined in function %s", label->name,
                LLVMGetValueName2(b->fn, &(size_t){}));
    }
  }
}

void builder_label_list_free(builder b) {
  struct label *label;
  slist_foreach(&b->labels, label) {
    sdsfree(label->name);
    free(label);
  }
  slist_free(&b->labels);
  slist_init(&b->labels);
}

void builder_destroy(builder b) {
  LLVMDisposeBuilder(b->builder);
  LLVMDisposeModule(b->module);
  LLVMContextDispose(b->context);
  builder_label_list_free(b);
}

/**
 * @brief relocate the extern declaration, 
 * 
 * @param b 
 * @param n 
 * @return typed_value 
 */
typed_value builder_relocate_declaration(builder b, astn n) {
  assert(n->type == ast_declaration);
  if (n->declaration.storage_class == TOK_KW_EXTERN) {
    // relocate extern declaration
    astn exist = parser_symtab_find(b->symtab, parse_declaration_get_ident(n));
    assert(exist);
    n = exist;
    log_debug("relocate extern declaration %s", n->declaration.ident);
  }

  return build_declaration(b, n);
}

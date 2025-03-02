#include "ast.h"
#include "builder.h"
#include "lexer.h"
#include "parser.h"
#include "slist/slist.h"
#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>

[[gnu::constructor]] void init() {
  log_color_enable(true);
  log_set_level(LOG_LEVEL_DEBUG);
  log_debug("test_builder init");
}

int process_trans_unit(struct lexer *lexer) {
  struct parser parser;
  parser_from_lexer(&parser, lexer);
  astn u = parse_translation_unit(&parser);
  struct builder b;
  builder_create(&b, "test", u);
  astn d;
  slist_foreach(&parser.symtab, d) { build_declaration(&b, d); }
  LLVMVerifyModule(b.module, LLVMAbortProcessAction, NULL);
  LLVMDumpModule(b.module);
  LLVMDisposeModule(b.module);
  LLVMContextDispose(b.context);
  parser_destory(&parser);
  ast_free(u);
  return 0;
}

void tbc_extern_multi_type() {
  struct lexer lexer;
  lexer_from_string(&lexer, "int v;double dv;float fv;char cv;");
  process_trans_unit(&lexer);

  lexer_destroy(&lexer);
}

void tbc_storages() {
  struct lexer lexer;
  lexer_from_string(&lexer, "static int v;extern int ev; int x2;");
  process_trans_unit(&lexer);

  lexer_destroy(&lexer);
}

void tbc_func() {
  struct lexer lexer;
  lexer_from_string(&lexer, "void f();void f2(int a,char b);void f3(void);");
  process_trans_unit(&lexer);
  lexer_destroy(&lexer);
}

void tbc_vafunc(){
  struct lexer lexer;
  lexer_from_string(&lexer, "void f(int a,char b,short c,...);");
  process_trans_unit(&lexer);
  lexer_destroy(&lexer);
}
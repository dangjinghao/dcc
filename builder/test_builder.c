#include "ast.h"
#include "builder.h"
#include "lexer.h"
#include "log/log.h"
#include "parser.h"
#include "slist/slist.h"
#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>

[[gnu::constructor]] void init() {
  log_color_enable(true);
  log_set_level(LOG_LEVEL_TRACE);
  log_trace("test_builder init");
}

int process_trans_unit(struct lexer *lexer) {
  struct parser parser;
  parser_from_lexer(&parser, lexer);
  astn u = parse_translation_unit(&parser);
  struct builder b;
  builder_create(&b, "test");
  astn d;
  parser_symtab_remove_weak_symbols(&parser.symtab);
  parser_reorder_strong_symbols(&parser.symtab);
  slist_foreach(&parser.symtab, d) { build_declaration(&b, d); }
  LLVMVerifyModule(b.module, LLVMAbortProcessAction, NULL);
  char *ir = LLVMPrintModuleToString(b.module);
  puts(ir);
  LLVMDisposeMessage(ir);
  builder_destroy(&b);
  parser_destory(&parser);
  ast_free(u);
  return 0;
}

void tbc_entry(char *code) {
  struct lexer lexer;
  lexer_from_string(&lexer, code);
  process_trans_unit(&lexer);
  lexer_destroy(&lexer);
}

void tbc_extern_multi_type() { tbc_entry("int v;double dv;float fv;char cv;"); }

void tbc_storages() {
  tbc_entry("extern int xv;static int v;float x2;int xv;");
}

void tbc_func() { tbc_entry("void f();void f2(int a,char b);void f3(void);"); }

void tbc_vafunc() { tbc_entry("void f(int a,char b,short c,...);"); }

void tbc_ptr() { tbc_entry("void* ptr;"); }

void tbc_struct() {
  tbc_entry("struct s {int a;char b; struct{short s; int d;}s;} s; struct s "
            "refs; struct{char c; "
            "char*s; struct{short s;}s;} abss;");
}
void tbc_multi_subscope_extern() {
  tbc_entry("int F1(){extern Fe();{extern Fe();}} int F2(){extern Fe(); "
            "{extern Fe();}}");
}

void tbc_func_def() { tbc_entry("char* f();char* f(){int a = 1;}"); }

void tbc_add(){
  tbc_entry("void F(){int a = 6.9f + 2;}");
}
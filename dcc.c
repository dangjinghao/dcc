#include "builder/builder.h"
#include "lexer/lexer.h"
#include "log/log.h"
#include "parser/parser.h"
#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int dcc_trans_unit(struct lexer *lexer) {
  struct parser parser;
  parser_new_from_lexer(&parser, lexer);
  astn u = parse_translation_unit(&parser);
  struct builder b;
  builder_new(&b, "<stdin>", &parser.symtab);
  slist symtab = parser_gen_symtab(&parser);
  build_trans_unit(&b, symtab);

  char *ir = LLVMPrintModuleToString(b.module);
  puts(ir);
  LLVMDisposeMessage(ir);

  char *msg = NULL;
  if (LLVMVerifyModule(b.module, LLVMReturnStatusAction, &msg)) {
    log_error("ir code verification failed: %s", msg);
  }
  LLVMDisposeMessage(msg);
  builder_destroy(&b);
  parser_destory(&parser);
  ast_free(u);
  return 0;
}

int main() {

  struct lexer lexer;
  log_set_level(LOG_LEVEL_INFO);
  log_color_enable(true);
  lexer_new_from_fp(&lexer, stdin);
  dcc_trans_unit(&lexer);
  lexer_destroy(&lexer);

  return 0;
}
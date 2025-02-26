#include "builder.h"
#include "convert/convert.h"
#include "parser.h"
#include "sds/sds.h"
#include "slist/slist.h"

[[gnu::constructor]] void init() {
  log_color_enable(true);
  log_set_level(LOG_LEVEL_DEBUG);
  log_debug("test_builder init");
}

int process_trans_unit(struct lexer *lexer) {
  struct parser parser;
  parser_from_lexer(&parser, lexer);
  parse_translation_unit(&parser);

  astn d;
  sds buf = sdsempty();
  slist_foreach(&parser.symtab, d) {
    buf = convert_ast_to_json(d, buf, false);
    printf("%s\n", buf);
    sdsclear(buf);
  }
  sdsfree(buf);
  parser_destory(&parser);
  return 0;
}

void tbc_extern_multi_decl() {
  struct lexer lexer;
  lexer_from_string(&lexer, "void F(){extern int A;A = 1;} int A;");
  process_trans_unit(&lexer);

  lexer_destroy(&lexer);
}
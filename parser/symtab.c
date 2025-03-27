#include "log/log.h"
#include "parser.h"

astn parser_symtab_find(slist symtab, sds id) {
  astn n;
  slist_foreach(symtab, n) {
    if (sdscmp(n->declaration.ident, id) == 0) {
      return n;
    }
  }
  return NULL;
}

/**
 * @brief Add a declaration to the symtab, usually used for merge extern symbol and symbol definition in codegen stage
 * 
 * @param parser 
 * @param n 
 */
void parser_symtab_add(parser parser, astn n) {
  assert(n->type == ast_declaration);
  if (n->declaration.storage_class == TOK_KW_EXTERN) {
    if (parser_symtab_find(&parser->symtab, n->declaration.ident)) {
      log_debug(
          "same name symbol already exists, skip to add extern symbol: %s",
          n->declaration.ident);
      return;
    } else {
      log_debug("add extern symbol to symtab: %s", n->declaration.ident);
    }
  } else {
    astn existing_symbol =
        parser_symtab_find(&parser->symtab, n->declaration.ident);
    if (existing_symbol &&
        existing_symbol->declaration.storage_class != TOK_KW_EXTERN) {
      log_panic(
          "same name symbol already exists, stop to add strong symbol: %s",
          n->declaration.ident);
    } else if (existing_symbol &&
               existing_symbol->declaration.storage_class == TOK_KW_EXTERN) {
      log_debug("remove extern symbol: %s", n->declaration.ident);
      slist_remove(&parser->symtab, existing_symbol);
    }
    log_debug("add strong symbol to symtab: %s", n->declaration.ident);
  }
  slist_add_head(&parser->symtab, n);
}

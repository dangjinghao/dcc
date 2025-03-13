#include "grammar.h"
#include "parser.h"

/**
 * @brief Add a declaration to the symtab, usually used for merge extern symbol and symbol definition in codegen stage
 * 
 * @param parser 
 * @param n 
 */
void parser_symtab_add(parser parser, astn n) {
  assert(n->type == ast_declaration);
  if (g_get_declaration_specifier(n)->ctype.storage == TOK_KW_EXTERN) {
    log_debug("add weak symbol to symtab: %s", n->declaration.ident);
    slist_add_tail(&parser->symtab, n);
  } else {
    log_debug("add strong symbol to symtab: %s", n->declaration.ident);
    slist_add_head(&parser->symtab, n);
  }
}

/**
 * @brief remove repeated weak symbols from symtab, 
 * 
 * @param symtab 
 * @param until 
 * @return true 
 * @return false 
 */
static bool parser_symtab_symbol_exist_until(slist symtab, astn until) {
  sds id = until->declaration.ident;
  astn n;
  slist_foreach(symtab, n) {
    assert(n->type == ast_declaration);
    // traverse until extern symbol
    if (n == until) {
      break;
    }

    if (sdscmp(n->declaration.ident, id) == 0) {
      return true;
    }
  }
  return false;
}

/**
 * @brief the order of the strong symbol is reversed, so we need to reorder it
 * 
 */
slist parser_symtab_reorder_strong_symbols(slist symtab) {
  struct slist new_symtab;
  slist_init(&new_symtab);
  astn n;
  slist_foreach(symtab, n) {
    if (g_get_declaration_specifier(n)->ctype.storage == TOK_KW_EXTERN) {
      log_trace("reorder: add extern symbol to new symtab: %s",
                n->declaration.ident);
      slist_add_tail(&new_symtab, n);
    } else {
      log_trace("reorder: add strong symbol to new symtab: %s",
                n->declaration.ident);
      slist_add_head(&new_symtab, n);
    }
  }
  slist_free(symtab);
  *symtab = new_symtab;
  return symtab;
}

void parser_symtab_remove_weak_symbols(slist symtab) {
  astn n;
  slist_foreach(symtab, n) {
    if (g_get_declaration_specifier(n)->ctype.storage != TOK_KW_EXTERN) {
      continue;
    }
    if (parser_symtab_symbol_exist_until(symtab, n)) {
      log_debug("remove weak symbol from symtab: %s", n->declaration.ident);
      slist_remove(symtab, n);
    }
  }
}
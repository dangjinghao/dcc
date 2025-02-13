#include "ast.h"
#include "chable/hash_table.h"
#include "convert/convert.h"
#include "ext/ext.h"
#include "grammar.h"
#include "lexer.h"
#include "log/log.h"
#include "parser.h"
#include "sds/sds.h"
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
astn parse_external_declaration(struct parser *parser) {
  assert(g_is_external_declaration_firstset(parser));
  BUILDING();
  // try-resoume algorithm
}

static uint64_t chable_hash(const void *data, struct chable *table) {
  astn astdata = (astn)data;
  assert(astdata->type == ast_declaration);
  return sdshash(astdata->declaration.ident);
}

static int chable_declaration_cmp(const void *a, const void *b) {
  astn asta = (astn)a;
  astn astb = (astn)b;
  assert(asta->type == ast_declaration && astb->type == ast_declaration);
  return sdscmp(asta->declaration.ident, astb->declaration.ident);
}
/**
 * @brief using the chable to store all declarations
 * 
 * @param parser 
 * @return astn 
 */
astn parse_translation_unit(struct parser *parser) {
  assert(g_is_translation_unit_firstset(parser));
  if (parser->current_token == TOK_EOF) {
    return NULL;
  }
  astn astobj = ast_new(ast_trans_unit);
  chable_default(&astobj->trans_unit.symtab, chable_hash,
                 chable_declaration_cmp);
  while (g_is_external_declaration_firstset(parser)) {
    astn d = parse_external_declaration(parser);
    assert(d->type == ast_declaration);
    chable_insert(&astobj->trans_unit.symtab, d);
  }
  return astobj;
}

static void parse_set_type_qualifier(ctype tn, enum type_qualifier qualifier) {
  tn->qualifier |= qualifier;
}

void parse_set_normal_type_specifier(ctype t, struct parser *parser) {
  enum tok_type tok = parser->current_token;
  enum tok_type prev_type = t->type;
  if (g_is_sign_tok(tok)) {
    if (t->signint != TOK_UNKNOWN) {
      compiler_error(parser->lexer, "re-signed type");
    }
    t->signint = tok;
  } else if (prev_type == TOK_UNKNOWN) {
    t->type = tok;
  } else if ((g_is_int_family_tok(prev_type) && g_is_int_family_tok(tok))) {
    // to support long int / short int
    if (convert_token_type_to_size(tok) >
        convert_token_type_to_size(prev_type)) {
      t->type = tok;
    }
  }
  log_error("invalid type: current_tok: %s,prev_tok: %s",
            convert_token_type_to_string(tok),
            convert_token_type_to_string(prev_type));

  parser_consume(parser);
}

/* {<declaration-specifier>}+ */
ctype parse_declaration_specifiers(struct parser *parser) {
  assert(g_is_declaration_specifier_firstset(parser));
  ctype tn = ctype_new();

  while (g_is_declaration_specifier_firstset(parser)) {
    if (g_is_type_qualifier_firstset(parser)) {
      parse_set_type_qualifier(
          tn, convert_token_type_to_qualifier(parser->current_token));
      parser_consume(parser);
    } else if (g_is_storage_class_specifier_firstset(parser)) {
      if (tn->storage != TOK_UNKNOWN) {
        compiler_error(parser->lexer,
                       "storage class specifier redefined: %s -> %s",
                       lexer_token_to_string(tn->storage),
                       lexer_token_to_string(parser->current_token));
        exit(EXIT_FAILURE);
      }
      parser_consume(parser);
    } else if (g_is_type_specifier_firstset(parser)) {
      if (g_is_typedef_name_firstset(parser)) {
        BUILDING();
      } else if (g_is_struct_or_union_specifier(parser)) {
        BUILDING();
      } else if (g_is_enum_specifier(parser)) {
        // we should convert enum to int when we meet it
        BUILDING();
      }
    }
  }
  // special case for unsigned
  if (tn->signint != TOK_UNKNOWN && tn->type == TOK_UNKNOWN) {
    tn->type = TOK_KW_INT;
  }
  return tn;
}

astn parse_declaration(struct parser *parser) {
  assert(g_is_declaration_firstset(parser));
  ctype type = parse_declaration_specifiers(parser);

  BUILDING();
}

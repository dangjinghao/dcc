#include "ast.h"
#include "convert/convert.h"
#include "dynarray/dynarray.h"
#include "grammar.h"
#include "lexer.h"
#include "log/log.h"
#include "parser.h"
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
astn parse_external_declaration(parser parser) {
  assert(g_is_external_declaration_firstset(parser));
  BUILDING();
  // try-resoume algorithm
}

/**
 * @brief using the chable to store all declarations
 * 
 * @param parser 
 * @return dynnarray
 */
astn parse_translation_unit(parser parser) {
  assert(g_is_translation_unit_firstset(parser));
  if (parser->current_token == TOK_EOF) {
    return NULL;
  }
  astn n = ast_new(ast_trans_unit);
  while (g_is_external_declaration_firstset(parser)) {
    parse_external_declaration(parser);
  }
  dynarray_copy(&n->trans_unit.declarations, &parser->idtab);
  return n;
}

static void parse_set_type_qualifier(ctype tn, enum type_qualifier qualifier) {
  tn->qualifier |= qualifier;
}

void parse_set_normal_type_specifier(ctype t, parser parser) {
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
ctype parse_declaration_specifiers(parser parser) {
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
      tn->storage = parser->current_token;
      parser_consume(parser);
    } else {
      assert(g_is_type_specifier_firstset(parser));
      if (g_is_typedef_name_firstset(parser)) {
        BUILDING();
      } else if (g_is_struct_or_union_specifier(parser)) {
        BUILDING();
      } else if (g_is_enum_specifier(parser)) {
        // we should convert enum to int when we meet it
        BUILDING();
      } else {
        parse_set_normal_type_specifier(tn, parser);
        parser_consume(parser);
      }
    }
  }
  // special case for unsigned
  if (tn->signint != TOK_UNKNOWN && tn->type == TOK_UNKNOWN) {
    tn->type = TOK_KW_INT;
  }
  return tn;
}

astn parse_initializer(parser parser) {
  assert(g_is_initializer_firstset(parser));
  if (parser->current_token == '{') {
    BUILDING();
  }
  return parse_assign_expr(parser);
}

astn parse_init_declarator(parser parser, ctype decl_specs) {
  assert(g_is_init_declarator_firstset(parser));
  astn n = ast_new(ast_declaration);
  astn declarator = parse_declarator(parser, decl_specs);
  n->declaration.ident = parse_remove_declarator_ident(declarator);
  n->declaration.type_chain = declarator;
  if (parser->current_token == '=') {
    parser_consume(parser);
    n->declaration.extdata = parse_initializer(parser);
  }
  return n;
}

astn parse_declaration(parser parser, astn symtab) {
  assert(g_is_declaration_firstset(parser));
  ctype decl_specs = parse_declaration_specifiers(parser);
  // we would reduce the grammar complexity,
  // e.g. int A,*B=0,(*C)(int,char); -> int A; int *B=0; int (*C)(int,char);
  astn init_declarator;
  if (g_is_init_declarator_firstset(parser))
    init_declarator = parse_init_declarator(parser, decl_specs);
  while (parser->current_token == ',') {
    parser_consume(parser);
    init_declarator = parse_init_declarator(parser, decl_specs);
  }

  parser_consume_with(parser, ';');
  BUILDING();
}

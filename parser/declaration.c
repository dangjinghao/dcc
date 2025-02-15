#include "ast.h"
#include "convert/convert.h"
#include "dynarray/dynarray.h"
#include "grammar.h"
#include "lexer.h"
#include "log/log.h"
#include "macro/macro.h"
#include "parser.h"
#include "sds/sds.h"
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
  astn n = ast_new(ast_block);
  while (g_is_external_declaration_firstset(parser)) {
    parse_external_declaration(parser);
  }
  return n;
}

static void parse_set_type_qualifier(struct ctype *tn,
                                     enum type_qualifier qualifier) {
  tn->qualifier |= qualifier;
}

void parse_set_normal_type_specifier(struct ctype *t, parser parser) {
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
  } else {
    log_panic("invalid type: current_tok: %s,prev_tok: %s",
              convert_token_type_to_string(tok),
              convert_token_type_to_string(prev_type));
  }
}

/* {<declaration-specifier>}+ */
astn parse_declaration_specifiers(parser parser) {
  assert(g_is_declaration_specifier_firstset(parser));
  astn n = ast_new(ast_ctype);
  struct ctype *tn = &n->ctype;

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
  return n;
}

astn parse_initializer(parser parser) {
  assert(g_is_initializer_firstset(parser));
  if (parser->current_token == '{') {
    BUILDING();
  }
  return parse_assign_expr(parser);
}

/**
 * @brief get inversed pointers chain
 * 
 * @param parser 
 * @param pointers 
 * @return dynarray 
 */
dynarray parse_pointers(parser parser, dynarray pointers) {
  assert(g_is_pointer_firstset(parser));

  while (g_is_pointer_firstset(parser)) {
    parser_consume(parser);
    astn p = ast_new(ast_ctype);
    p->ctype.type = '*';
    while (g_is_type_qualifier_firstset(parser)) {
      parse_set_type_qualifier(
          &p->ctype, convert_token_type_to_qualifier(parser->current_token));
      parser_consume(parser);
    }
    dynarray_add(pointers, &p);
  }
  return pointers;
}

dynarray parse_direct_declarator(parser parser, dynarray type_chain) {
  assert(g_is_direct_declarator_firstset(parser));
  if (parser->current_token == TOK_IDENT) {
    astn id = parse_ident(parser);
    // promise the ident is the 1st element
    assert(dynarray_size(type_chain) == 0);
    dynarray_add(type_chain, &id);
  } else if (parser->current_token == '(') {
    parser_consume(parser);
    parse_declarator(parser, type_chain);
    parser_consume_with(parser, ')');
  }
  while (parser->current_token == '[' || parser->current_token == '(') {
    if (parser->current_token == '[') {
      parser_consume(parser);
      BUILDING();
      if (parser->current_token != ']') {
      }
      parser_consume_with(parser, ']');
    } else {
      parser_consume(parser);
      if (parser->current_token != ')') {
        BUILDING();
      }
      parser_consume_with(parser, ')');
    }
  }
  return type_chain;
}

/**
 * @brief Save the type declaration as reverse order:
 * int *const *(*a)[10] -> a * [10] * const* int
 * 
 * @param parser 
 * @param declaration 
 * @return astn 
 */
dynarray parse_declarator(parser parser, dynarray type_chain) {
  assert(g_is_declarator_firstset(parser));
  struct dynarray inversed_pointers;
  dynarray_init(&inversed_pointers, sizeof(astn), 0);
  if (g_is_pointer_firstset(parser)) {
    parse_pointers(parser, &inversed_pointers);
  }
  parse_direct_declarator(parser, type_chain);
  astn *ref;
  dynarray_foreach_reverse(&inversed_pointers, ref) {
    dynarray_add(type_chain, ref);
  }
  dynarray_free(&inversed_pointers);
  return type_chain;
}

sds parse_remove_type_chain_ident(dynarray type_chain) {
  astn first = dynarray_get(type_chain, 0);
  if (first->type != ast_ident) {
    // for the abstract declarator, it doesn't have an identifier
    log_debug("abstract declarator");
    return NULL;
  }
  dynarray_pop_head(type_chain, NULL);
  return first->ident;
}

astn parse_init_declarator(parser parser, astn decl_specs) {
  assert(g_is_init_declarator_firstset(parser));
  astn n = ast_new(ast_declaration);
  struct dynarray *type_chain = n->declaration.type_chain;
  parse_declarator(parser, type_chain);
  dynarray_add(type_chain, &decl_specs);
  n->declaration.ident = parse_remove_type_chain_ident(type_chain);
  n->declaration.type_chain = type_chain;
  if (parser->current_token == '=') {
    parser_consume(parser);
    n->declaration.extdata = parse_initializer(parser);
  }
  return n;
}

astn parse_declaration(parser parser) {
  assert(g_is_declaration_firstset(parser));
  astn decl_specs = parse_declaration_specifiers(parser);
  // we would reduce the grammar complexity,
  // e.g. int A,*B=0,(*C)(int,char); -> int A; int *B=0; int (*C)(int,char);
  astn init_declarator;
  astn n = ast_new(ast_block);
  if (g_is_init_declarator_firstset(parser)) {
    init_declarator = parse_init_declarator(parser, decl_specs);
    dynarray_add(n->block.decls, &init_declarator);
  }
  while (parser->current_token == ',') {
    parser_consume(parser);
    init_declarator = parse_init_declarator(parser, decl_specs);
    dynarray_add(n->block.decls, &init_declarator);
  }
  parser_consume_with(parser, ';');
  return n;
}
#include "ast.h"
#include "convert/convert.h"
#include "dynarray/dynarray.h"
#include "grammar.h"
#include "lexer.h"
#include "log/log.h"
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
  astn n = ast_new(ast_trans_unit);
  while (g_is_external_declaration_firstset(parser)) {
    parse_external_declaration(parser);
  }
  dynarray_copy(&n->trans_unit.declarations, &parser->idtab);
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
  }
  log_error("invalid type: current_tok: %s,prev_tok: %s",
            convert_token_type_to_string(tok),
            convert_token_type_to_string(prev_type));

  parser_consume(parser);
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

astn parse_pointers(parser parser, astn declaration) {
  assert(g_is_pointer_firstset(parser));
  while (g_is_pointer_firstset(parser)) {
    parser_consume_with(parser, '*');
    astn p = ast_new(ast_ctype);
    p->ctype.type = '*';
    while (g_is_type_qualifier_firstset(parser)) {
      parse_set_type_qualifier(&p->ctype, parser->current_token);
      parser_consume(parser);
    }
    dynarray_add(&declaration->declaration.type_chain, &p);
  }
  return declaration;
}

astn parse_direct_declarator(parser parser, astn declaration) {
  assert(g_is_direct_declarator_firstset(parser));
  // temporary save <identifier> and add it in the end to
  // make sure that the <identifier> is the last element in the chain
}

astn parse_declarator(parser parser, astn declaration) {
  assert(g_is_declarator_firstset(parser));
  assert(declaration->type == ast_declaration);
  if (g_is_pointer_firstset(parser)) {
    parse_pointers(parser, declaration);
  }

  return parse_direct_declarator(parser, declaration);
}

sds parse_remove_declarator_ident(astn declarator) {
  assert(declarator->type == ast_declaration);
  astn last;
  dynarray_pop(&declarator->declaration.type_chain, &last);
  if(last->type != ast_ident) {
    // for the abstract declarator, it doesn't have an identifier
    return NULL;
  }
  sds r = last->ident;
  parser_free_ast(last);
  return r;
}

astn parse_init_declarator(parser parser, astn decl_specs) {
  assert(g_is_init_declarator_firstset(parser));
  astn n = ast_new(ast_declaration);
  dynarray_default(&n->declaration.type_chain, sizeof(astn));
  dynarray_add(&n->declaration.type_chain, &decl_specs);
  astn declarator = parse_declarator(parser, n);
  n->declaration.ident = parse_remove_declarator_ident(declarator);
  n->declaration.type_chain = declarator->declaration.type_chain;
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
  if (g_is_init_declarator_firstset(parser))
    init_declarator = parse_init_declarator(parser, decl_specs);
  while (parser->current_token == ',') {
    parser_consume(parser);
    init_declarator = parse_init_declarator(parser, decl_specs);
  }
  // TODO: add those declarations to the symbol table
  BUILDING();
  parser_consume_with(parser, ';');
}

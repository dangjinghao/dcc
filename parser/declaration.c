#include "ast.h"
#include "convert/convert.h"
#include "grammar.h"
#include "lexer.h"
#include "log/log.h"
#include "macro/macro.h"
#include "parser.h"
#include "sds/sds.h"
#include "slist/slist.h"
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief using the chable to store all declarations
 * @param parser 
 * @return dynnarray
 */
astn parse_translation_unit(parser parser) {
  assert(g_is_translation_unit_firstset(parser));

  astn n = ast_new(ast_list);
  parser->global_block = n;
  parser->global_uid = parser->local_uid = 1;
  while (g_is_external_declaration_firstset(parser)) {
    parse_external_declaration(parser, n);
    parser->local_uid = 1;
  }
  parser_consume_with(parser, TOK_EOF);
  parser->global_block = NULL;
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
      }
      tn->storage = parser->current_token;
      parser_consume(parser);
    } else {
      assert(g_is_type_specifier_firstset(parser));
      if (g_is_typedef_name_firstset(parser)) {
        tn->type = TOK_KW_TYPEDEF;
        astn ref = ast_new(ast_ref);
        ref->ref = parser_get_typedef_by_type_name(
            parser, parser->lexer->lex_token._ident);
        tn->user_defined_type = ref;
        assert(parser->current_token == TOK_IDENT);
        sdsfree(parser->lexer->lex_token._ident);
        parser_consume(parser);
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
 * @brief
 * 
 * @param parser 
 * @param pointers 
 * @return slist 
 */
slist parse_pointers(parser parser, slist pointers) {
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
    slist_add_head(pointers, p);
  }
  return pointers;
}

/**
 * @brief we will reuse the init-declarator to reduce 
 * the complexity of implementation
 * 
 * @param parser 
 * @return astn 
 */
astn parse_parameter_declaration(parser parser) {
  assert(g_is_parameter_declaration_firstset(parser));
  astn decl_specs = parse_declaration_specifiers(parser);
  return parse_init_declarator(parser, decl_specs, true);
}

slist parse_parameter_type_list(parser p, astn astp) {
  assert(astp->type == ast_list);
  slist params = &astp->list;
  assert(g_is_parameter_list_firstset(p));
  astn param = parse_parameter_declaration(p);
  slist_add_tail(params, param);
  while (p->current_token == ',') {
    parser_consume(p);
    if (g_is_parameter_declaration_firstset(p)) {
      param = parse_parameter_declaration(p);
      slist_add_tail(params, param);
    } else if (p->current_token == TOK_SYM_VARARGS) {
      parser_consume(p);
      param = ast_new(ast_ctype);
      param->ctype.type = TOK_SYM_VARARGS;
      slist_add_tail(params, param);
      break;
    } else {
      compiler_error(p->lexer, "Unexpected token: %s",
                     convert_token_type_to_string(p->current_token));
    }
  }
  return params;
}

slist parse_direct_declarator(parser parser, slist type_chain) {
  // assert(g_is_direct_declarator_firstset(parser));
  if (parser->current_token == TOK_IDENT) {
    astn id = parse_ident(parser);
    // promise the ident is the 1st element
    assert(slist_length(type_chain) == 0);
    slist_add_tail(type_chain, id);
  } else if (parser->current_token == '(') {
    parser_consume(parser);
    parse_declarator(parser, type_chain);
    parser_consume_with(parser, ')');
  }
  while (parser->current_token == '[' || parser->current_token == '(') {
    if (parser->current_token == '[') {
      parser_consume(parser);

      astn content_type = ast_new(ast_expr_unary);
      content_type->unary.op = '[';
      if (parser->current_token != ']') {
        content_type->unary.expr = parse_constant_expr(parser);
      }
      parser_consume_with(parser, ']');
      slist_add_tail(type_chain, content_type);
    } else {
      parser_consume(parser);
      astn content_type = ast_new(ast_list);
      if (parser->current_token != ')') {
        parse_parameter_type_list(parser, content_type);
      }
      parser_consume_with(parser, ')');
      slist_add_tail(type_chain, content_type);
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
slist parse_declarator(parser parser, slist type_chain) {
  // assert(g_is_declarator_firstset(parser));
  struct slist pointers;
  slist_init(&pointers);
  if (g_is_pointer_firstset(parser)) {
    parse_pointers(parser, &pointers);
  }
  parse_direct_declarator(parser, type_chain);
  slist_concat(type_chain, &pointers);
  slist_free(&pointers);
  return type_chain;
}

sds parse_remove_type_chain_ident(slist type_chain) {
  astn first = slist_peek_head(type_chain);
  if (first->type != ast_ident) {
    // for the abstract declarator, it doesn't have an identifier
    log_debug("abstract declarator");
    return NULL;
  }
  slist_pop_head(type_chain);
  sds ident = sdsdup(first->ident);
  ast_free(first);
  return ident;
}
/* delay_alloc_uid is used in parsing function parameters, to avoid allocating wrong block uid */
astn parse_init_declarator(parser parser, astn decl_specs,
                           bool delay_alloc_id) {
  // assert(g_is_init_declarator_firstset(parser));
  astn n = ast_new(ast_declaration);
  slist type_chain = &n->declaration.type_chain;
  parse_declarator(parser, type_chain);
  slist_add_tail(type_chain, decl_specs);
  n->declaration.ident = parse_remove_type_chain_ident(type_chain);
  if (g_get_function_params(n) && parser->current_token != '{') {
    // we should not use g_is_function_declaration(n) because the function is waiting for parsing.
    // if it is a function declaration, we should add an `extern` storage to
    // distinguish it from a function definition simply.
    // we put this process in there because the later parser_declare_new_symbol needs the 
    // extern to determine whether it is a function declaration or not.
    log_debug("add extern storage specifier to function declaration: %s",
              n->declaration.ident);
    
    decl_specs->ctype.storage = TOK_KW_EXTERN;
  }

  if (!delay_alloc_id) {
    // this symbol declaration would be delayed to the function definition process
    // and it should only be used in the function parameter parse process
    parser_declare_new_symbol(parser, n);
  }
  if (parser->current_token == '=') {
    if (g_get_function_params(n)) {
      compiler_error(parser->lexer,
                     "function declaration should not have an initializer");
    }
    parser_consume(parser);
    n->declaration.extdata = parse_initializer(parser);
  } else if (parser->current_token == '{') {
    // function definition
    if (!parser_is_current_block_global(parser)) {
      compiler_error(parser->lexer,
                     "function definition is not allowed in non-global scope");
    }
    parser_push_scope(parser);
    parser->current_function_block = n;
    // add params to the scope
    astn ps = g_get_function_params(n);
    if (!ps) {
      compiler_error(parser->lexer,
                     "function definition without parameters declaration");
    }
    astn p;
    slist_foreach(&ps->list, p) {
      sds id = p->declaration.ident;
      if (!id) {
        compiler_error(parser->lexer,
                       "there is a function parameter without an identifier");
      }
      parser_declare_new_symbol(parser, p);
    }

    // function body
    n->declaration.extdata = parse_compound_statement(parser);
    parser->current_function_block = NULL;
    parser_pop_scope(parser);
  }
  return n;
}

/**
 * @brief Because we would reduce many delcarators to a single declaration,
 * e.g. int a,b,c; -> int a; int b; int c;
 * so we need pass a block to store all the declarations to make it easier
 * 
 * @param parser 
 * @param current_block 
 * @return astn 
 */
astn parse_external_declaration(parser parser, astn current_block) {
  assert(g_is_external_declaration_firstset(parser));
  assert(current_block->type == ast_list);
  astn decl_specs = parse_declaration_specifiers(parser);
  // e.g. int A,*B=0,(*C)(int,char); -> int A; int *B=0; int (*C)(int,char);
  astn init_declarator;
  if (g_is_init_declarator_firstset(parser)) {
    init_declarator = parse_init_declarator(parser, decl_specs, false);
    assert(init_declarator->type == ast_declaration);
    slist_add_tail(&current_block->list, init_declarator);
    if (g_is_function_definition(init_declarator)) {
      return current_block;
    }
  }
  while (parser->current_token == ',') {
    parser_consume(parser);
    init_declarator =
        parse_init_declarator(parser, ast_copy(decl_specs), false);
    assert(init_declarator->type == ast_declaration);
    slist_add_tail(&current_block->list, init_declarator);
  }
  parser_consume_with(parser, ';');
  return current_block;
}

astn parse_specifier_qualifier(parser parser) {
  assert(g_is_specifier_qualifier_firstset(parser));
  astn spec_qual = parse_declaration_specifiers(parser);
  return spec_qual;
}

/**
 * @brief reuse the parse_declaration_specifiers and parse_declarator
 * 
 * @param parser 
 * @return astn 
 */
slist parse_type_name(parser parser, slist type_chain) {
  assert(g_is_type_name_firstset(parser));
  astn spec_qual = parse_specifier_qualifier(parser);

  parse_declarator(parser, type_chain);
  slist_add_tail(type_chain, spec_qual);
  if (parse_remove_type_chain_ident(type_chain)) {
    compiler_error(parser->lexer, "<type-name> should not have an identifier.");
  }
  return type_chain;
}

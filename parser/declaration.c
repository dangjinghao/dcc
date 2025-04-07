#include "ast.h"
#include "convert/convert.h"
#include "grammar.h"
#include "lexer.h"
#include "log/log.h"
#include "parser.h"
#include "sds/sds.h"
#include "slist/slist.h"
#include "token.h"
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
  astn n = ast_new(ast_trans_unit);
  parser->uidcnt = 1;
  while (g_is_external_declaration_firstset(parser)) {
    parse_external_declaration(parser, &n->trans_unit.list);
  }
  parser_consume_with(parser, TOK_EOF);
  return n;
}

astn parse_struct_declarator(parser parser, astn decl_specs) {
  assert(g_is_struct_declarator_firstset(parser));
  astn n = ast_new(ast_declaration);
  slist type_chain = &n->declaration.type_chain;
  if (g_is_declarator_firstset(parser)) {
    parse_declarator(parser, type_chain);
    slist_add_tail(type_chain, decl_specs);
    parser_unfold_type_chain(parser, type_chain);
    n->declaration.ident = parse_type_chain_pop_ident(type_chain);
  }
  if (parser->current_token == ':') {
    parser_consume_with(parser, ':');
    n->declaration.extdata = parse_expr_const_int(parser);
  }
  return n;
}

slist parse_struct_declaration(parser parser, slist member_declarations) {
  assert(g_is_struct_declaration_firstset(parser));
  astn decl_specs = parse_specifier_qualifiers(parser);
  astn struct_declarator = parse_struct_declarator(parser, decl_specs);
  slist_add_tail(member_declarations, struct_declarator);
  while (parser->current_token == ',') {
    parser_consume(parser);
    struct_declarator = parse_struct_declarator(parser, ast_copy(decl_specs));
    slist_add_tail(member_declarations, struct_declarator);
  }
  parser_consume_with(parser, ';');
  return member_declarations;
}

astn parse_struct_or_union_specifier(parser parser) {
  assert(g_is_struct_or_union_specifier(parser));
  astn n = ast_new(ast_struct_or_union_declaration);
  n->struct_or_union_declaration.type = parser->current_token;
  parser_consume(parser);
  if (parser->current_token == TOK_IDENT) {
    n->struct_or_union_declaration.ident = parser->lexer->lex_token._ident;
    parser_consume(parser);
  }
  astn ref;
  if (parser->current_token == '{') {
    parser_consume(parser);
    // forward add to tag table for avoiding self ref in member
    if (n->struct_or_union_declaration.ident) {
      parser_new_tag(parser, n);
    }
    while (g_is_struct_declaration_firstset(parser)) {
      parse_struct_declaration(
          parser, &n->struct_or_union_declaration.member_declarations);
    }
    parser_consume_with(parser, '}');
    log_debug("add uid to struct declaration");
    ref = n;
  } else {
    // struct declaration, check if it existing
    if (!n->struct_or_union_declaration.ident) {
      compiler_error(parser->lexer,
                     "struct/union declaration without an identifier");
    }
    ref = parser_scope_all_find_ident(n->struct_or_union_declaration.ident,
                                      &parser->tagtab);
    if (!ref) {
      compiler_error(parser->lexer, "Undefined struct/union declaration: %s",
                     n->struct_or_union_declaration.ident);
    }
    if (ref->struct_or_union_declaration.type !=
        n->struct_or_union_declaration.type) {
      compiler_error(parser->lexer, "struct/union type mismatch: %s",
                     n->struct_or_union_declaration.ident);
    }
    ast_free(n);
  }
  n = ast_new(ast_ref);
  n->ref = ref;
  return n;
}

astn parse_enumerator(parser parser, slist enumerators, long *enum_counter) {
  assert(g_is_enumerator_firstset(parser));
  astn n = ast_new(ast_enumerator);
  n->enumerator.ident = parser->lexer->lex_token._ident;
  parser_consume(parser);
  if (parser->current_token == '=') {
    parser_consume(parser);
    astn const_expr = parse_expr_const_int(parser);
    n->enumerator.value = parser_expr_eval_const_int(const_expr);
    *enum_counter = n->enumerator.value;
    ast_free(const_expr);
  } else {
    n->enumerator.value = *enum_counter;
  }
  *enum_counter += 1;
  slist_add_tail(enumerators, n);
  // because the enumerator would be added to tab, if we freee the enumeration related with this,
  // it would cause the enumerator to be freed
  parser_new_enumerator(parser, ast_copy(n));
  return n;
}

astn parse_enumeration(parser parser) {
  assert(g_is_enum_specifier(parser));
  parser_consume_with(parser, TOK_KW_ENUM);
  astn n = ast_new(ast_enumeration);
  if (parser->current_token == TOK_IDENT) {
    n->enumeration.ident = parser->lexer->lex_token._ident;
    parser_consume(parser);
  }
  if (parser->current_token == '{') {
    parser_consume(parser);
    long enum_counter = 0;
    while (g_is_enumerator_firstset(parser)) {
      parse_enumerator(parser, &n->enumeration.enumerators, &enum_counter);
      if (parser->current_token == ',') {
        parser_consume(parser);
      } else {
        break;
      }
    }
    parser_consume_with(parser, '}');
  }
  astn ref = parser_new_tag(parser, n);
  n = ast_new(ast_ref);
  n->ref = ref;
  return n;
}

static void parse_declaration_specifiers_set_normal_type(struct ctype *t,
                                                         parser parser) {
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
    if (lexer_token_get_sizeof(tok) > lexer_token_get_sizeof(prev_type)) {
      t->type = tok;
    }
  } else {
    log_panic("invalid type: current_tok: %s,prev_tok: %s",
              convert_repr_token(tok), convert_repr_token(prev_type));
  }
}

/* {<declaration-specifier>}+ */
astn parse_declaration_specifiers(parser parser) {
  assert(g_is_declaration_specifier_firstset(parser));
  astn n = ast_new(ast_ctype);
  struct ctype *tn = &n->ctype;
  while (g_is_declaration_specifier_firstset(parser)) {
    if (g_is_type_qualifier_firstset(parser)) {
      tn->qualifier |= convert_cast_token_to_qualifier(parser->current_token);
      parser_consume(parser);
    } else if (g_is_storage_class_specifier_firstset(parser)) {
      if (tn->storage != TOK_UNKNOWN) {
        char str1[16];
        strcpy(str1, lexer_token_get_str(tn->storage));
        char str2[16];
        strcpy(str2, lexer_token_get_str(parser->current_token));
        compiler_error(parser->lexer,
                       "storage class specifier redefined: %s -> %s", str1,
                       str2);
      }
      tn->storage = parser->current_token;
      parser_consume(parser);
    } else {
      if (tn->type != TOK_UNKNOWN) {
        // tn->type != TOK_UNKNOWN is used to solve this problem: typedef int A; A A;
        // if we don't use this flag, A would be recognized as a type specifier twice
        // it's unexpected situation
        break;
      }
      assert(g_is_type_specifier_firstset(parser));
      if (g_is_typedef_name_firstset(parser)) {
        tn->type = TOK_KW_TYPEDEF;
        astn ref = ast_new(ast_ref);
        ref->ref =
            parser_lookup_typedef(parser, parser->lexer->lex_token._ident);
        tn->user_defined_type = ref;
        assert(parser->current_token == TOK_IDENT);
        sdsfree(parser->lexer->lex_token._ident);
        parser_consume(parser);
      } else if (g_is_struct_or_union_specifier(parser)) {
        tn->type = parser->current_token;
        tn->user_defined_type = parse_struct_or_union_specifier(parser);
      } else if (g_is_enum_specifier(parser)) {
        tn->type = parser->current_token;
        tn->user_defined_type = parse_enumeration(parser);
      } else {
        parse_declaration_specifiers_set_normal_type(tn, parser);
        parser_consume(parser);
      }
    }
  }
  if (tn->type == TOK_UNKNOWN) {
    log_trace("set default type to int");
    tn->type = TOK_KW_INT;
  }
  if (tn->signint == TOK_UNKNOWN && g_is_int_family_tok(tn->type)) {
    log_trace("set default signint to signed");
    tn->signint = TOK_KW_SIGNED;
  }
  return n;
}

static astn parse_initializer_body(parser parser);
/**
 * @brief consume the final optional `,`, even though it is not in the <initializer-list> procedure
 * 
 * @param parser 
 * @return astn 
 */
astn parse_initializer_list(parser parser) {
  assert(g_is_initializer_list_firstset(parser));
  astn n = ast_new(ast_initializer_list);
  while (g_is_initializer_firstset(parser)) {
    slist_add_tail(&n->initializer_list.list, parse_initializer_body(parser));
    if (parser->current_token == ',') {
      parser_consume(parser);
    } else {
      break;
    }
  }
  return n;
}
static astn parse_initializer_body(parser parser) {
  if (parser->current_token == '{') {
    parser_consume(parser);
    astn result = parse_initializer_list(parser);
    parser_consume_with(parser, '}');
    return result;
  } else {
    return parse_expr_assign(parser);
  }
}
astn parse_initializer(parser parser) {
  assert(g_is_initializer_firstset(parser));
  astn n = ast_new(ast_initializer);
  n->initializer.init = parse_initializer_body(parser);
  return n;
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
      p->ctype.qualifier |=
          convert_cast_token_to_qualifier(parser->current_token);
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
  assert(decl_specs->type == ast_ctype &&
         decl_specs->ctype.storage == TOK_UNKNOWN);
  return parse_init_declarator(parser, decl_specs, true);
}

slist parse_parameter_type_list(parser p, astn astp) {
  assert(astp->type == ast_parameters);
  slist params = &astp->parameters.list;
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

      slist_add_tail(params, g_new_varargs_param());
      break;
    } else {
      compiler_error(p->lexer, "Unexpected token: %s",
                     convert_repr_token(p->current_token));
    }
  }
  return params;
}

slist parse_direct_declarator(parser parser, slist type_chain) {
  // assert(g_is_direct_declarator_firstset(parser));
  if (parser->current_token == TOK_IDENT) {
    astn id = parse_expr_ident(parser);
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
        content_type->unary.expr = parse_expr_const_int(parser);
      }
      parser_consume_with(parser, ']');
      slist_add_tail(type_chain, content_type);
    } else {
      parser_consume(parser);
      astn parameters = ast_new(ast_parameters);
      if (parser->current_token == ')') {
        // int F(); in C language, it means F with any parameters
        // but this way had been deprecated in C23
        // we should rewrite to `int F(void)`
        slist_add_tail(&parameters->parameters.list, g_new_void_param());
      } else {
        parse_parameter_type_list(parser, parameters);
      }
      parser_consume_with(parser, ')');
      slist_add_tail(type_chain, parameters);
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

sds parse_type_chain_pop_ident(slist type_chain) {
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
  log_trace("move ctype.storage to declaration.storage_class");
  n->declaration.storage_class = decl_specs->ctype.storage;
  decl_specs->ctype.storage = TOK_UNKNOWN;
  parser_unfold_type_chain(parser, type_chain);
  n->declaration.ident = parse_type_chain_pop_ident(type_chain);
  if (g_get_function_params(n) && parser->current_token != '{') {
    // we should not use g_is_function_declaration(n) because the function is waiting for parsing.
    // if it is a function declaration, we should add an `extern` storage to
    // distinguish it from a function definition simply.
    // we put this process in there because the later parser_declare_new_symbol needs the
    // extern to determine whether should it be added to the symtab.
    log_trace("add extern storage specifier to function declaration: %s",
              parser_declaration_get_ident(n));

    n->declaration.storage_class = TOK_KW_EXTERN;
  }

  if (!delay_alloc_id) {
    // this symbol declaration would be delayed to the function definition process
    // and it should only be used in the function parameter parse process
    n->declaration.scope_ref = parser->function_scope;
    parser_new_declaration(parser, n);
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
    if (!parser_scope_is_current_global(parser)) {
      compiler_error(parser->lexer,
                     "function definition is not allowed in non-global scope");
    }
    parser_push_scope(parser);
    parser->function_scope = n;
    // add params to the scope
    astn ps = g_get_function_params(n);
    if (!ps) {
      compiler_error(parser->lexer,
                     "function definition without parameters declaration");
    }
    astn p;
    slist_foreach(&ps->parameters.list, p) {
      sds id = p->declaration.ident;
      if (g_is_void_param(p) || g_is_varargs_param(p)) {
        break;
      }
      if (!id) {
        compiler_error(parser->lexer,
                       "there is a function parameter without an identifier");
      }
      p->declaration.scope_ref = parser->function_scope;
      parser_new_declaration(parser, p);
    }

    // function body
    n->declaration.extdata = parse_statement_compound(parser);
    parser->function_scope = NULL;
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
 */
void parse_external_declaration(parser parser, slist block) {
  assert(g_is_external_declaration_firstset(parser));
  astn decl_specs = parse_declaration_specifiers(parser);
  // e.g. int A,*B=0,(*C)(int,char); -> int A; int *B=0; int (*C)(int,char);
  astn init_declarator;
  if (g_is_init_declarator_firstset(parser)) {
    // in parse_init_declartor, we would modify the decl_specs->ctype.storage
    // so we have to copy it to avoid the side effect
    init_declarator =
        parse_init_declarator(parser, ast_copy(decl_specs), false);
    assert(init_declarator->type == ast_declaration);
    slist_add_tail(block, init_declarator);
    if (g_is_function_definition(init_declarator)) {
      return;
    }
    while (parser->current_token == ',') {
      parser_consume(parser);
      init_declarator =
          parse_init_declarator(parser, ast_copy(decl_specs), false);
      assert(init_declarator->type == ast_declaration);
      slist_add_tail(block, init_declarator);
    }
    // we have to free the decl_specs here because it is not used exactly
    ast_free(decl_specs);
  } else {
    // struct or union declaration without identifier
    assert(decl_specs->type == ast_ctype &&
           decl_specs->ctype.storage == TOK_UNKNOWN);
    astn n = ast_new(ast_declaration);
    slist type_chain = &n->declaration.type_chain;
    slist_add_tail(type_chain, decl_specs);
    parser_unfold_type_chain(parser, type_chain);
    slist_add_tail(block, n);
  }
  parser_consume_with(parser, ';');
}

astn parse_specifier_qualifiers(parser parser) {
  assert(g_is_specifier_qualifier_firstset(parser));
  astn spec_qual = parse_declaration_specifiers(parser);
  assert(spec_qual->type == ast_ctype &&
         spec_qual->ctype.storage == TOK_UNKNOWN);
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
  astn spec_qual = parse_specifier_qualifiers(parser);

  parse_declarator(parser, type_chain);
  slist_add_tail(type_chain, spec_qual);
  parser_unfold_type_chain(parser, type_chain);
  if (parse_type_chain_pop_ident(type_chain)) {
    compiler_error(parser->lexer, "<type-name> should not have an identifier.");
  }
  return type_chain;
}

sds parser_declaration_get_ident(astn n) {
  switch (n->type) {
  case ast_declaration:
    return n->declaration.ident;
  case ast_struct_or_union_declaration:
    return n->struct_or_union_declaration.ident;
  case ast_enumeration:
    return n->enumeration.ident;
  case ast_enumerator:
    return n->enumerator.ident;
  default:
    log_panic("unexpected ast declartion type: %s",
              convert_repr_ast_type(n->type));
  }
  return NULL;
}

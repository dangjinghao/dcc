
#include "ast.h"
#include "grammar.h"
#include "lexer.h"
#include "macro/macro.h"
#include "parser.h"
#include "sds/sds.h"
#include <assert.h>
#include <stdbool.h>
static astn __parse_primary_expr_combine_str(parser parser) {
  assert(parser->current_token == TOK_LIT_STRING);
  astn node = ast_new(ast_expr_primary);
  node->primary.type = TOK_LIT_STRING;
  node->primary.v = parser->lexer->lex_token;
  parser_consume(parser);
  while (parser->current_token == TOK_LIT_STRING) {
    node->primary.v._str =
        sdscatsds(node->primary.v._str, parser->lexer->lex_token._str);
    sdsfree(parser->lexer->lex_token._str);
    parser_consume(parser);
  }
  return node;
}

astn parse_primary_expr(parser parser) {
  assert(g_is_primary_expression_firstset(parser));
  astn node = NULL;
  switch (parser->current_token) {
  case TOK_LIT_STRING:
    node = __parse_primary_expr_combine_str(parser);
    break;
  // gnu switch case range extension
  case (__TOK_LIT_START + 1)...(TOK_LIT_STRING - 1):
  case (TOK_LIT_STRING + 1)...(TOK_SYM_LEQ - 1):
    node = ast_new(ast_expr_primary);
    node->primary.type = parser->current_token;
    node->primary.v = parser->lexer->lex_token;
    parser_consume(parser);
    break;
  case TOK_IDENT:
    node = ast_new(ast_ident);
    node->ident = parser->lexer->lex_token._ident;
    parser_consume(parser);
    break;
  case '(':
    parser_consume(parser);
    node = parse_comma_expr(parser);
    parser_consume_with(parser, ')');
    break;
  default:
    compiler_error(parser->lexer, "Unexpected token %s",
                   lexer_token_to_string(parser->current_token));
  }

  return node;
}

// static ast_node_ptr parse_unary_advanced_postfix(parser parser) {
// }

astn parse_unary_postfix(parser parser) {
  static int postfix_ops[] = {
      TOK_SYM_SELF_INC, TOK_SYM_SELF_DEC, TOK_SYM_ARROW, '[', '(', '.'};
  astn v = parse_primary_expr(parser);
  while (true) {
    if (ARRAY_IN(postfix_ops, parser->current_token, EQ_EQ)) {
      astn node = ast_new(ast_expr_unary);
      // TODO: support advanced postfix
      node->unary.op = parser->current_token;
      node->unary.postfix = true;
      parser_consume(parser);
      node->unary.expr = v;
      v = node;
    } else {
      break;
    }
  }
  return v;
}

astn parse_unary_prefix(parser parser) {
  static int prefix_uops[] = {
      TOK_SYM_SELF_INC,
      TOK_SYM_SELF_DEC,
      TOK_KW_SIZEOF,
      '-',
      '+',
      '!',
      '~',
      '*',
      '&',
  };

  if (ARRAY_IN(prefix_uops, parser->current_token, EQ_EQ)) {
    astn node = ast_new(ast_expr_unary);
    node->unary.op = parser->current_token;
    node->unary.postfix = false;
    parser_consume(parser);
    node->unary.expr = parse_unary_postfix(parser);
    return node;
  }

  return parse_unary_postfix(parser);
}

astn parse_unary(parser parser) {
  assert(g_is_unary_expression_firstset(parser));
  return parse_unary_prefix(parser);
}

struct infix_parselet {
  int token;
  int prec;
  bool right_assoc;
  astn (*handle)(astn left, parser parser, struct infix_parselet *self);
};

static astn __binop_normal_handle(astn left, parser parser,
                                  struct infix_parselet *self) {
  astn n = ast_new(ast_expr_binop);
  n->binop.op = self->token;
  n->binop.lhs = left;
  n->binop.rhs = __parse_assign_expr(parser, self->right_assoc ? self->prec - 1
                                                               : self->prec);
  return n;
}

static astn __binop_ternary_handle(astn left, parser parser,
                                   struct infix_parselet *self) {
  astn n = ast_new(ast_expr_ternary);
  n->ternary.cond = left;
  n->ternary._t = parse_assign_expr(parser);
  parser_consume_with(parser, ':');
  n->ternary._f = parse_assign_expr(parser);
  return n;
}

struct infix_parselet infix_parselets[] = {
    {
        '%',
        210,
        false,
        __binop_normal_handle,
    },
    {
        '/',
        210,
        false,
        __binop_normal_handle,
    },
    {
        '*',
        210,
        false,
        __binop_normal_handle,
    },
    {
        '-',
        200,
        false,
        __binop_normal_handle,
    },
    {
        '+',
        200,
        false,
        __binop_normal_handle,
    },
    {
        TOK_SYM_RSHIFT,
        190,
        false,
        __binop_normal_handle,
    },
    {
        TOK_SYM_LSHIFT,
        190,
        false,
        __binop_normal_handle,
    },
    {
        '>',
        180,
        false,
        __binop_normal_handle,
    },
    {
        TOK_SYM_GEQ,
        180,
        false,
        __binop_normal_handle,
    },
    {
        '<',
        180,
        false,
        __binop_normal_handle,
    },
    {
        TOK_SYM_LEQ,
        180,
        false,
        __binop_normal_handle,
    },
    {
        TOK_SYM_NEQ,
        170,
        false,
        __binop_normal_handle,
    },
    {
        TOK_SYM_EQ,
        170,
        false,
        __binop_normal_handle,
    },
    {
        '&',
        160,
        false,
        __binop_normal_handle,
    },
    {
        '^',
        150,
        false,
        __binop_normal_handle,
    },
    {
        '|',
        140,
        false,
        __binop_normal_handle,
    },
    {
        TOK_SYM_LOGIC_AND,
        130,
        false,
        __binop_normal_handle,
    },
    {
        TOK_SYM_LOGIC_OR,
        120,
        false,
        __binop_normal_handle,
    },
    {
        '?',
        110,
        true,
        __binop_ternary_handle,
    },
    {
        '=',
        110,
        true,
        __binop_normal_handle,
    },
    {
        TOK_SYM_SELF_RSHIFT,
        110,
        true,
        __binop_normal_handle,
    },
    {
        TOK_SYM_SELF_LSHIFT,
        110,
        true,
        __binop_normal_handle,
    },
    {
        TOK_SYM_SELF_ADD,
        110,
        true,
        __binop_normal_handle,
    },
    {
        TOK_SYM_SELF_SUB,
        110,
        true,
        __binop_normal_handle,
    },
    {
        TOK_SYM_SELF_MUL,
        110,
        true,
        __binop_normal_handle,
    },
    {
        TOK_SYM_SELF_DIV,
        110,
        true,
        __binop_normal_handle,
    },
    {
        TOK_SYM_SELF_MOD,
        110,
        true,
        __binop_normal_handle,
    },
    {
        TOK_SYM_SELF_BIT_OR,
        110,
        true,
        __binop_normal_handle,
    },
    {
        TOK_SYM_SELF_BIT_XOR,
        110,
        true,
        __binop_normal_handle,
    },
    {
        TOK_SYM_SELF_BIT_AND,
        110,
        true,
        __binop_normal_handle,
    },

};

/* Pratt algorithm parser */
astn __parse_assign_expr(parser parser, int ctx_prec) {
  astn left = parse_unary(parser);

  while (true) {
    int infix_token = parser->current_token;
    if (!infix_token)
      break;

#define EQ_TOKEN(a, b) ((a).token == (b))
    struct infix_parselet *parselet =
        ARRAY_GET(infix_parselets, infix_token, EQ_TOKEN);
#undef EQ_TOKEN

    if (!parselet) {
      break;
    }
    if (parselet->prec <= ctx_prec) {
      break;
    }
    parser_consume(parser);
    left = parselet->handle(left, parser, parselet);
  }

  return left;
}

astn parse_assign_expr(parser parser) { return __parse_assign_expr(parser, 0); }

/**
 * @brief the comma expression is a sequence of expressions separated by commas,
 * it enjoys the lowest precedence in C operators.
 * We split it from pratt expr parser because we need to handle the comma-separated variables initialization
 * which has another syntax.
 * @param parser 
 * @return comma 
 */
astn parse_comma_expr(parser parser) {
  astn node = parse_assign_expr(parser);
  while (parser->current_token == ',') {
    astn n = ast_new(ast_expr_binop);
    n->binop.op = ',';
    n->binop.lhs = node;
    parser_consume(parser);
    n->binop.rhs = parse_assign_expr(parser);
    node = n;
  }
  return node;
}
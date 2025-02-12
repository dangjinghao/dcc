
#include "ast.h"
#include "lexer.h"
#include "macro/macro.h"
#include "parser.h"
#include <assert.h>
#include <stdbool.h>

astn parse_primary_expr(struct parser *parser) {
  // TODO: support (expr)
  astn node = ast_new(ast_expr_primary);
  assert((parser->current_token > __TOK_LIT_START &&
          parser->current_token < __TOK_LIT_END) ||
         parser->current_token == TOK_IDENT);
  node->value.type = parser->current_token;
  node->value.v = parser->lexer->lex_token;
  parser_consume(parser);
  return node;
}

// static ast_node_ptr parse_unary_advanced_postfix(struct parser *parser) {
// }

astn parse_unary_postfix(struct parser *parser) {
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

astn parse_unary_prefix(struct parser *parser) {
  static int prefix_ops[] = {
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
  if (ARRAY_IN(prefix_ops, parser->current_token, EQ_EQ)) {
    astn node = ast_new(ast_expr_unary);
    node->unary.op = parser->current_token;
    node->unary.postfix = false;
    parser_consume(parser);
    node->unary.expr = parse_unary_postfix(parser);
    return node;
  }

  return parse_unary_postfix(parser);
}

astn parse_unary(struct parser *parser) { return parse_unary_prefix(parser); }

struct infix_parselet {
  int token;
  int prec;
  bool right_assoc;
  astn (*handle)(astn left, struct parser *parser, struct infix_parselet *self);
};

static astn __binop_normal_handle(astn left, struct parser *parser,
                                  struct infix_parselet *self) {
  astn n = ast_new(ast_expr_binop);
  n->binop.op = self->token;
  n->binop.lhs = left;
  n->binop.rhs =
      __parse_expr(parser, self->right_assoc ? self->prec - 1 : self->prec);
  return n;
}

static astn __binop_ternary_handle(astn left, struct parser *parser,
                                   struct infix_parselet *self) {
  astn n = ast_new(ast_expr_ternary);
  n->ternary.cond = left;
  n->ternary._t = parse_expr(parser);
  parser_consume_with(parser, ':');
  n->ternary._f = parse_expr(parser);
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
    {
        ',',
        50,
        false,
        __binop_normal_handle,
    },
};

/* Pratt algorithm parser */
astn __parse_expr(struct parser *parser, int ctx_prec) {
  astn left = parse_unary(parser);

  while (true) {
    const int infix_token = parser->current_token;
    if (!infix_token)
      break;
    struct infix_parselet *parselet = NULL;
    for (size_t i = 0; i < ARRAY_SIZE(infix_parselets); i++) {
      if (infix_parselets[i].token == infix_token) {
        parselet = &infix_parselets[i];
        break;
      }
    }
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

astn parse_expr(struct parser *parser) { return __parse_expr(parser, 0); }

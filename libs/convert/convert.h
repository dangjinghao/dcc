#ifndef CONVERT_H
#define CONVERT_H
#include "ast.h"
char convert_decode_char(char *c, char **endptr);
sds convert_expr_obj_to_repr(astn n, sds buf);
enum type_qualifier convert_token_type_to_qualifier(enum tok_type tok);
char *convert_token_type_to_string(enum tok_type tok);
size_t convert_token_type_to_size(enum tok_type t);
char *convert_ast_type_to_string(enum ast_type t);
#endif
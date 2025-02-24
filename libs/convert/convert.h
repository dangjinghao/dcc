#ifndef CONVERT_H
#define CONVERT_H
#include "ast.h"
char convert_decode_char(char *c, char **endptr);
sds convert_ast_to_json(astn n, sds buf,bool shallow);
enum type_qualifier convert_token_type_to_qualifier(enum tok_type tok);
char *convert_token_type_enum_to_repr(enum tok_type tok);
size_t convert_token_type_to_size(enum tok_type t);
char *convert_ast_type_enum_to_repr(enum ast_type t);
char* convert_type_qualifier(enum type_qualifier t);
#endif
#ifndef CONVERT_H
#define CONVERT_H
#include "ast.h"
sds convert_repr_jsonify_ast(astn n, sds buf, bool shallow);
char convert_repr_decode_char(char *c, char **endptr);
char *convert_repr_token(enum tok_type tok);
char *convert_repr_ast_type(enum ast_type t);
char *convert_repr_enum_type_qualifier(enum type_qualifier t);

enum type_qualifier convert_cast_token_to_qualifier(enum tok_type tok);

#endif
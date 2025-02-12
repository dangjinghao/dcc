#ifndef CONVERT_H
#define CONVERT_H
#include "ast.h"
char convert_decode_char(char *c, char **endptr);
sds convert_expr_obj_to_repr(astn n, sds buf);
#endif
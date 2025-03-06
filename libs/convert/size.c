#include "convert.h"

size_t convert_token_type_to_size(enum tok_type t) {
  switch (t) {
  case TOK_KW_INT:
    return sizeof(int);
  case TOK_KW_VOID:
  case TOK_KW_CHAR:
    return sizeof(char);
  case TOK_KW_FLOAT:
    return sizeof(float);
  case TOK_KW_DOUBLE:
    return sizeof(double);
  case TOK_KW_LONG:
    return sizeof(long);
  case TOK_KW_SHORT:
    return sizeof(short);
  default:
    log_panic("unsupported type: %s", convert_token_type_enum_to_repr(t));
  }
  return 0;
}
#include "convert.h"

enum type_qualifier convert_cast_token_to_qualifier(enum tok_type tok) {
  switch (tok) {
  case TOK_KW_CONST:
    return TYPE_QUAL_CONST;
  case TOK_KW_VOLATILE:
    return TYPE_QUAL_VOLATILE;
  // case TOK_KW_RESTRICT:
  //   return TYPE_QUAL_RESTRICT;
  // case TOK_KW_INLINE:
  //   return TYPE_QUAL_INLINE;
  default:
    log_panic("invalid type qualifier token");
    return TYPE_QUAL_NONE;
  }
}

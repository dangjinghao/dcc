#include "typed_value.h"
#include <stdlib.h>

typed_value typed_value_new(LLVMValueRef v, slist type_chain) {
  typed_value tv = calloc(1, sizeof(struct typed_value));
  tv->v = v;
  tv->type_chain = *type_chain;
  return tv;
}
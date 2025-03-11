#include "typed_value.h"
#include <stdlib.h>

llvm_typed_value llvm_typed_value_new(LLVMValueRef v, slist type_chain) {
  llvm_typed_value tv = calloc(1, sizeof(struct llvm_typed_value));
  tv->v = v;
  tv->type_chain = *type_chain;
  return tv;
}
#ifndef TYPE_H
#define TYPE_H
#include "slist/slist.h"
#include <llvm-c/Types.h>
typedef struct llvm_typed_value {
  LLVMValueRef v;
  /** the physical llvm type chain of the declaration,
   * if it in the declaration, it maybe a ptr points to the type chain of the declaration
   */
  struct slist type_chain;
} *llvm_typed_value;

llvm_typed_value llvm_typed_value_new(LLVMValueRef v, slist type_chain);

#endif
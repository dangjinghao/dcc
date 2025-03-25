#ifndef TYPE_H
#define TYPE_H
#include "slist/slist.h"
#include <llvm-c/Types.h>
typedef struct typed_value {
  LLVMValueRef v;
  /** the physical llvm type chain of the declaration, should be used only in non-base type processing
   * if it is in a declaration, it may be a ptr which points to the type chain of the declaration
   */
  struct slist type_chain;
} *typed_value;

typed_value typed_value_new(LLVMValueRef v, slist type_chain);

#endif
#ifndef BUILDER_H
#define BUILDER_H
#include "ast.h"
#include <llvm-c/Types.h>
typedef struct builder {
  astn ast;
  LLVMBuilderRef builder;
  LLVMModuleRef module;
  LLVMContextRef context;
} *builder;

#endif
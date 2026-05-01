#ifndef DCC_H
#define DCC_H
#include <stdnoreturn.h>

// tokenizer.c

typedef struct {
  char *name;
  char *contents;
} DFile;

noreturn void error(char *fmt, ...);

#endif
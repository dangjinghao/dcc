#include "dcc.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct DFile *current_file;

static bool str_startswith(const char *str, const char *substr) {
  return strncmp(str, substr, strlen(substr)) == 0;
}

// Throw an error message with this format:
// <filename>:<line>: a = b + c;
//                    ^ <error message>
static void verror_at(const char *filename, const char *input,
                      const int line_no, const char *loc, const char *fmt,
                      va_list ap) {
  // Find the line which containing loc
  const char *line = loc;
  while (input < line && line[-1] != '\n') {
    line -= 1;
  }

  const char *end = loc;
  while (*end && *end != '\n') {
    end += 1;
  }
  int indent = fprintf(stderr, "%s:%d: ", filename, line_no);
  fprintf(stderr, "%.*s\n", (int)(end - line), line);

  int pos = loc - line + indent;

  fprintf(stderr, "%*s^ ", pos, "");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
}

static void error_at(const char *loc, const char *fmt, ...) {
  int line_no = 1;
  for (const char *p = current_file->contents; p < loc; p++)
    if (*p == '\n')
      line_no++;

  va_list ap;
  va_start(ap, fmt);
  verror_at(current_file->name, current_file->contents, line_no, loc, fmt, ap);
  exit(1);
}

void error(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

int main() {
  struct DFile *fp = calloc(1, sizeof(struct DFile));
  fp->contents = "int main()\n{return 0;}";
  fp->name = __FILE_NAME__;
  current_file = fp;
  const char *loc;

  loc = fp->contents + 15;
  error_at(loc, "loc: %s", loc);
  return 0;
}
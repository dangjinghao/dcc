#include "dcc.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

DFile *current_file;

static bool str_startswith(char *str, char *substr) {
  return strncmp(str, substr, strlen(substr)) == 0;
}

// Throw an error message with this format:
// <filename>:<line>: a = b + c;
//                    ^ <error message>
static void verror_at(char *filename, char *input, int line_no, char *loc,
                      char *fmt, va_list ap) {
  // Find the line which containing loc
  char *line = loc;
  while (input < line && line[-1] != '\n') {
    line -= 1;
  }

  char *end = loc;
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

static void error_at(char *loc, char *fmt, ...) {
  int line_no = 1;
  for (char *p = current_file->contents; p < loc; p++)
    if (*p == '\n')
      line_no++;

  va_list ap;
  va_start(ap, fmt);
  verror_at(current_file->name, current_file->contents, line_no, loc, fmt, ap);
  exit(1);
}

#define isodigit(c) ('0' <= (c) && (c) <= '7')

// Read the escaped char after `\`
// e.g. \56
//       ^
// e.g. \x56
//       ^
static int read_escaped_char(char *p, char **new_pos) {

  if (isodigit(*p)) {
    char *endpos = NULL;
    long v = strtol(p, &endpos, 8);
    *new_pos = endpos;
    return (int)v;
  }

  if (*p == 'x') {
    p += 1;
    char *endptr = NULL;
    long v = strtol(p, &endptr, 16);
    *new_pos = endptr;
    return (int)v;
  }

  *new_pos = p + 1;
  switch (*p) {
  case 'a':
    return '\a';
  case 'b':
    return '\b';
  case 't':
    return '\t';
  case 'n':
    return '\n';
  case 'v':
    return '\v';
  case 'f':
    return '\f';
  case 'r':
    return '\r';
  // [GNU] \e for the ASCII escape character is a GNU C extension.
  case 'e':
    return '\e';
  default:
    return *p;
  }
}

void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

int main() {
  char *p = "\12345";
  char *endp = NULL;
  int c = read_escaped_char(p, &endp);
  printf("%d %s\n", c, endp);
  p = "\1Ab";
  c = read_escaped_char(p, &endp);
  printf("%d %s\n", c, endp);
  return 0;
}
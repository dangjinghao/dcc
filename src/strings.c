#include "dcc.h"
#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Returns a heap-allocated formatted string. Caller owns the buffer.
char *format(const char *fmt, ...) {
  char *buf;
  size_t buflen;
  FILE *out = open_memstream(&buf, &buflen);

  va_list ap;
  va_start(ap, fmt);
  vfprintf(out, fmt, ap);
  va_end(ap);
  fflush(out);
  fclose(out);
  return buf;
}

// Returns a string representing `len` bytes of `s`, with non-printable
// characters shown as \xFF escapes. Returns heap memory (caller owns)
// except for the empty-string case which returns a static literal.
char *visual_bytes(const char *s, const size_t len) {
  if (len == 0 || s == NULL || !strlen(s)) {
    return "(null)";
  }
  char *buf;
  size_t buflen;
  FILE *out = open_memstream(&buf, &buflen);
  for (size_t i = 0; i < len; i++) {
    if (isprint(s[i])) {
      fwrite(&s[i], 1, 1, out);
    } else {
      // format: \xFF
      fprintf(out, "\\x%02hhX", (unsigned char)s[i]);
    }
  }
  fputc('\0', out);
  fflush(out);
  fclose(out);
  return buf;
}

bool str_endswith(const char *p, const char *q) {
  int len1 = strlen(p);
  int len2 = strlen(q);
  return (len1 >= len2) && !strcmp(p + len1 - len2, q);
}

bool str_endswithcase(const char *p, const char *q) {
  int len1 = strlen(p);
  int len2 = strlen(q);
  return (len1 >= len2) && !strcasecmp(p + len1 - len2, q);
}

bool str_startswith(const char *str, const char *substr) {
  return strncmp(str, substr, strlen(substr)) == 0;
}

bool str_startswithcase(const char *str, const char *substr) {
  return strncasecmp(str, substr, strlen(substr)) == 0;
}
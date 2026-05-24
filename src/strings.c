#include "dcc.h"
#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Takes a printf-style format string and returns a formatted string.
char *format(char *fmt, ...) {
  char *buf;
  size_t buflen;
  FILE *out = open_memstream(&buf, &buflen);

  va_list ap;
  va_start(ap, fmt);
  vfprintf(out, fmt, ap);
  va_end(ap);
  fclose(out);
  return buf;
}

char *visual_bytes(char *s, size_t len) {
  if (len == 0) {
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

bool str_endswith(char *p, char *q) {
  int len1 = strlen(p);
  int len2 = strlen(q);
  return (len1 >= len2) && !strcmp(p + len1 - len2, q);
}

bool str_startswith(char *str, char *substr) {
  return strncmp(str, substr, strlen(substr)) == 0;
}

bool str_startswithcase(char *str, char *substr) {
  return strncasecmp(str, substr, strlen(substr)) == 0;
}
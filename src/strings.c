#include "dcc.h"
#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

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

void strarray_push(StringArray *arr, char *s) {
  if (!arr->data) {
    arr->data = calloc(8, sizeof(char *));
    arr->capacity = 8;
  }

  if (arr->capacity == arr->len) {
    arr->data = realloc(arr->data, sizeof(char *) * arr->capacity * 2);
    arr->capacity *= 2;
    for (int i = arr->len; i < arr->capacity; i++)
      arr->data[i] = NULL;
  }

  arr->data[arr->len++] = s;
}
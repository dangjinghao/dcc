#include <stdlib.h>
/**
 * @brief input the decoded char part,
 * e.g. convert '\n' -> input 'n' -> return 0x10, \777 -> 777 -> 0x1ff
 * 
 * @param c 
 * @return char 
 */
char convert_decode_char(char *c, char **endptr) {
  if (endptr)
    *endptr = c + 1;
  switch (c[0]) {
  case 'n':
    return '\n';

  case '\\':
    return '\\';

  case 't':
    return '\t';

  case 'a':
    return '\a';

  case 'b':
    return '\b';

  case 'f':
    return '\f';

  case 'r':
    return '\r';

  case 'v':
    return '\v';

  case '\'':
    return '\'';

  case '\"':
    return '\"';

  case '?':
    return '\?';

  case 'x':
    return (char)strtol(c + 1, endptr, 16);

  default:
    // in fact, \0 is octal
    // octal
    return (char)strtol(c, endptr, 8);
  }
  return 0;
}
#ifndef MACRO_H
#define MACRO_H

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define CONCAT(x, y) x##y
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

#define ARRAY_IN(arr, x, eq)                                                   \
  ({                                                                           \
    bool result = false;                                                       \
    for (size_t i = 0; i < ARRAY_SIZE(arr); i++) {                             \
      if (eq(arr[i], x)) {                                                     \
        result = true;                                                         \
        break;                                                                 \
      }                                                                        \
    }                                                                          \
    result;                                                                    \
  })

#define EQ_EQ(a, b) ((a) == (b))

#endif
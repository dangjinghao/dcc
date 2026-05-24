#include "dcc.h"
#include <stdlib.h>

void ptrarray_push(PtrArray *arr, void *s) {
  if (!arr->data) {
    arr->data = calloc(8, sizeof(void *));
    arr->capacity = 8;
  }

  if (arr->capacity == arr->len) {
    arr->data = realloc(arr->data, sizeof(void *) * arr->capacity * 2);
    arr->capacity *= 2;
    for (int i = arr->len; i < arr->capacity; i++)
      arr->data[i] = NULL;
  }

  arr->data[arr->len++] = s;
}

// append_array should be end with NULL
void ptrarray_push_batch(PtrArray *arr, void **append_array) {
  for (void **p = append_array; *p; p++)
    ptrarray_push(arr, *p);
}

void ptrarray_push_batch2(PtrArray *arr, PtrArray *append_array) {
  for (size_t i = 0; i < append_array->len; i++) {
    ptrarray_push(arr, append_array->data[i]);
  }
}
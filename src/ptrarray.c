#include "dcc.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

void ptrarray_insert(PtrArray *arr, size_t idx, void *s) {
  if (!arr->data) {
    arr->data = malloc(8 * sizeof(void *));
    arr->capacity = 8;
  }

  if (arr->capacity == arr->len) {
    void **new_data = realloc(arr->data, sizeof(void *) * arr->capacity * 2);
    if (!new_data)
      abort();
    arr->data = new_data;
    arr->capacity *= 2;
  }
  assert(idx <= arr->len);
  if (idx == arr->len) {
    arr->data[arr->len++] = s;
    return;
  }

  memmove(&arr->data[idx + 1], &arr->data[idx],
          (arr->len - idx) * sizeof(void *));
  arr->data[idx] = s;
  arr->len++;
}

void ptrarray_push(PtrArray *arr, void *s) {
  ptrarray_insert(arr, arr->len, s);
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

void ptrarray_free(PtrArray *arr) {
  free(arr->data);
  arr->data = NULL;
  arr->capacity = 0;
  arr->len = 0;
}

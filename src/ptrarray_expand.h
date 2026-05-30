
#ifndef PTRARRAY_PTR_TYPE
#define PTRARRAY_PTR_TYPE void *
#endif

#ifndef PTRARRAY_PREFIX
#define PTRARRAY_PREFIX ptrarray
#endif

#define __PTRARRAY_CONCAT2(a, b) a##b
#define __PTRARRAY_CONCAT(a, b) __PTRARRAY_CONCAT2(a, b)

#define PTRARRAY_PREFIX_EXPAND(F) __PTRARRAY_CONCAT(PTRARRAY_PREFIX, F)

static inline void PTRARRAY_PREFIX_EXPAND(_push)(PtrArray *arr,
                                                 PTRARRAY_PTR_TYPE s) {
  ptrarray_push(arr, (void *)s);
}

static inline void PTRARRAY_PREFIX_EXPAND(_insert)(PtrArray *arr,
                                                   unsigned long idx,
                                                   PTRARRAY_PTR_TYPE s) {
  ptrarray_insert(arr, idx, (void *)s);
}

static inline void
PTRARRAY_PREFIX_EXPAND(_push_batch)(PtrArray *arr,
                                    PTRARRAY_PTR_TYPE *append_array) {
  ptrarray_push_batch(arr, (void **)append_array);
}

static inline void
PTRARRAY_PREFIX_EXPAND(_push_batch2)(PtrArray *arr, PtrArray *append_array) {
  ptrarray_push_batch2(arr, append_array);
}

static inline PTRARRAY_PTR_TYPE
PTRARRAY_PREFIX_EXPAND(_data)(PtrArray *arr, unsigned long idx) {
  if (arr->len <= idx) {
    return (void *)0;
  }
  return arr->data[idx];
}

#undef __PTRARRAY_CONCAT
#undef __PTRARRAY_CONCAT2
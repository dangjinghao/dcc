#ifndef SLIST_H
#define SLIST_H
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#define SLIST_MALLOC malloc
#define SLIST_FREE free
#define SLIST_ASSIGN_INIT { NULL, NULL }
typedef struct slist {
  struct slist *next;
  /* points to the last node (tail) in the head node */
  void *data;
} *slist;

/* Initialize an slist head node:
 *   list->next = NULL
 *   list->data = NULL
 */
static inline void slist_init(slist list) {
  list->next = NULL;
  list->data = NULL;
}

/* Return the n-th node in the list (starting from the head->next).
 * If n == -1, return list->data (which is the tail node).
 * if n == 0, return the head node.
 * If n is out of range, return NULL.
 */
static inline slist slist_get_list_node(slist list, long n) {
  if (n == -1) {
    return (slist)list->data;
  }
  slist p = list;
  while (n-- > 0) {
    if (p->next == NULL) {
      return NULL;
    }
    p = p->next;
  }
  return p;
}

/* Return true if the list is empty, otherwise false. */
static inline bool slist_empty(slist list) {
  assert(((list->next == NULL) && (list->data == NULL)) ||
         ((list->next != NULL) && (list->data != NULL)));
  return (list->next == NULL && list->data == NULL);
}

/* Insert a new node at the head of the list. Maintain the tail pointer. */
static inline void slist_add_head(slist list, void *data) {
  bool was_empty = slist_empty(list);
  slist new_node = (slist)SLIST_MALLOC(sizeof(struct slist));
  new_node->data = data;
  new_node->next = list->next;
  list->next = new_node;
  /* If the list was empty, set tail pointer to new_node. */
  if (was_empty) {
    list->data = new_node;
  }
}

/* Insert a new node at the tail of the list. Maintain the tail pointer. */
static inline void slist_add_tail(slist list, void *data) {
  slist new_node = (slist)SLIST_MALLOC(sizeof(struct slist));
  new_node->data = data;
  new_node->next = NULL;

  if (slist_empty(list)) {
    /* The list is empty, so the new node is both head and tail. */
    list->next = new_node;
  } else {
    /* Append to the current tail, then update tail to new_node. */
    ((slist)list->data)->next = new_node;
  }
  list->data = new_node;
}

/* Return the data from the head node, or NULL if the list is empty. */
static inline void *slist_peek_head(slist list) {
  return list->next ? list->next->data : NULL;
}

/* Return the data from the tail node, or NULL if the list is empty. */
static inline void *slist_peek_tail(slist list) {
  return list->data ? ((slist)list->data)->data : NULL;
}

/* Remove and return the data from the head node. */
static inline void *slist_pop_head(slist list) {
  if (!slist_empty(list)) {
    slist head = list->next;
    list->next = head->next;
    /* If this node was also the tail, reset tail to NULL. */
    if (list->data == head) {
      list->data = NULL;
    }
    void *data = head->data;
    head->data = head->next = NULL; // for safety, remove in the future
    SLIST_FREE(head);
    return data;
  }
  return NULL;
}

/* Remove and return the data from the tail node. */
static inline void *slist_pop_tail(slist list) {
  if (slist_empty(list)) {
    return NULL;
  }

  slist tail = (slist)list->data;
  /* If there's only one node in the list, clear everything out. */
  if (list->next == tail) {
    list->next = NULL;
    list->data = NULL;
    void *data = tail->data;
    tail->data = tail->next = NULL; // for safety, remove in the future
    SLIST_FREE(tail);
    return data;
  }

  /* Otherwise, find the node just before the tail. */
  slist prev = list->next;
  while (prev->next != tail) {
    prev = prev->next;
  }
  prev->next = NULL;
  list->data = prev;

  void *data = tail->data;
  tail->data = tail->next = NULL; // for safety, remove in the future
  SLIST_FREE(tail);
  return data;
}

/* Free all nodes (except the head node itself), resetting head and tail. */
static inline void slist_free(slist list) {
  slist node = list->next;
  while (node) {
    slist next = node->next;
    SLIST_FREE(node);
    node = next;
  }
  list->next = NULL;
  list->data = NULL;
}

/* Concatenate src list to dst list, emptying out src. */
static inline void slist_concat(slist dst, slist src) {
  if (slist_empty(src)) {
    return;
  }
  if (slist_empty(dst)) {
    *dst = *src;
    slist_init(src);
    return;
  }
  ((slist)dst->data)->next = src->next;
  dst->data = src->data;
  slist_init(src);
}

/* Iteration macro for convenience: 
 *   slist_foreach(mylist, pos) { ... }
 * where 'pos' is a pointer to the node's data.
 */

#define slist_foreach(list, d)                                                 \
  for (slist pos = (list)->next, tmp;                                          \
       pos && ((tmp = pos->next), d = pos->data, 1); pos = tmp)

static inline void slist_copy(slist dst, slist src) {
  void *data;
  slist_init(dst);
  slist_foreach(src, data) { slist_add_tail(dst, data); }
}

static inline size_t slist_length(slist list) {
  int len = 0;
  void *_;
  slist_foreach(list, _) { len++; }
  return len;
}

static inline void slist_remove(slist list, void *data) {
  for (slist p = list; p->next; p = p->next) {
    if (p->next->data == data) {
      slist tmp = p->next;
      p->next = p->next->next;
      if (tmp == list->data) {
        list->data = NULL;
        assert(p->next == NULL);
      }
      SLIST_FREE(tmp);
      return;
    }
  }
}

static inline bool slist_exists(slist list, void *data) {
  void *d;
  slist_foreach(list, d) {
    if (d == data) {
      return true;
    }
  }
  return false;
}

#endif
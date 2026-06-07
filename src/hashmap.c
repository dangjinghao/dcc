// This is an implementation of the open-addressing hash table.

#include "dcc.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

// Initial hash bucket size
#define INIT_SIZE 16

// Rehash if the usage exceeds 70% .
#define HIGH_WATERMARK 70

// We'll keep the usage below 50% after rehashing.
#define LOW_WATERMARK 50

// Represents a deleted hash entry
#define TOMBSTONE ((void *)-1)

static uint64_t fnv_hash(char *s, int len) {
  uint64_t hash = 0xcbf29ce484222325;
  for (int i = 0; i < len; i++) {
    hash *= 0x100000001b3;
    hash ^= (unsigned char)s[i];
  }
  return hash;
}

// Make room for new entires in a given hashmap by removing
// tombstones and possibly extending the bucket size.
static void rehash(HashMap *map) {
  // Compute the size of the new hashmap.
  int nkeys = 0;
  for (int i = 0; i < map->capacity; i++)
    if (hashmap_entry_valid(map->buckets + i))
      nkeys++;

  int cap = map->capacity;
  while ((nkeys * 100) / cap >= LOW_WATERMARK)
    cap = cap * 2;
  assert(cap > 0);

  // Create a new hashmap and copy all key-values.
  HashMap map2 = {};
  map2.buckets = calloc(cap, sizeof(HashEntry));
  map2.capacity = cap;

  for (int i = 0; i < map->capacity; i++) {
    HashEntry *ent = &map->buckets[i];
    if (hashmap_entry_valid(ent))
      hashmap_put2(&map2, ent->key, ent->keylen, ent->val);
  }

  assert(map2.used == nkeys);
  free(map->buckets);
  *map = map2;
}

static bool match(HashEntry *ent, char *key, int keylen) {
  return hashmap_entry_valid(ent) && ent->keylen == keylen &&
         memcmp(ent->key, key, keylen) == 0;
}

static HashEntry *get_entry(HashMap *map, char *key, int keylen) {
  if (!map->buckets)
    return NULL;

  uint64_t hash = fnv_hash(key, keylen);

  for (int i = 0; i < map->capacity; i++) {
    HashEntry *ent = &map->buckets[(hash + i) % map->capacity];
    if (match(ent, key, keylen))
      return ent;
    if (ent->key == NULL)
      return NULL;
  }
  unreachable();
}

static void init_map(HashMap *map) {
  map->buckets = calloc(INIT_SIZE, sizeof(HashEntry));
  map->capacity = INIT_SIZE;
  map->used = 0;
}

static HashEntry *get_or_insert_entry(HashMap *map, char *key, int keylen) {
  if (!map->buckets) {
    init_map(map);
  } else if ((map->used * 100) / map->capacity >= HIGH_WATERMARK) {
    rehash(map);
  }
  HashEntry *tombstone = NULL;
  uint64_t hash = fnv_hash(key, keylen);

  for (int i = 0; i < map->capacity; i++) {
    HashEntry *ent = &map->buckets[(hash + i) % map->capacity];

    if (ent->key == TOMBSTONE && !tombstone) {
      // find first tombstone
      tombstone = ent;
      continue;
    }
    if (match(ent, key, keylen))
      return ent;
    if (ent->key == NULL) {
      ent->key = key;
      ent->keylen = keylen;
      map->used++;
      return ent;
    }
  }
  if (tombstone) {
    // if we don't find an empty slot but we found a tombstone
    // use this tombstone
    // See: https://github.com/rui314/chibicc/issues/135
    tombstone->key = key;
    tombstone->keylen = keylen;
    map->used++;
    return tombstone;
  }
  unreachable();
}

void *hashmap_get(HashMap *map, char *key) {
  return hashmap_get2(map, key, strlen(key));
}

void *hashmap_get2(HashMap *map, char *key, int keylen) {
  HashEntry *ent = get_entry(map, key, keylen);
  return ent ? ent->val : NULL;
}

void hashmap_put(HashMap *map, char *key, void *val) {
  hashmap_put2(map, key, strlen(key), val);
}

void hashmap_put2(HashMap *map, char *key, int keylen, void *val) {
  HashEntry *ent = get_or_insert_entry(map, key, keylen);
  ent->val = val;
}

void hashmap_delete(HashMap *map, char *key) {
  hashmap_delete2(map, key, strlen(key));
}

void hashmap_delete2(HashMap *map, char *key, int keylen) {
  HashEntry *ent = get_entry(map, key, keylen);
  if (ent)
    ent->key = TOMBSTONE;
  map->used--;
}

void hashmap_destroy(HashMap *map) { free(map->buckets); }

void hashmap_clear(HashMap *map) {
  if (!map->buckets) {
    init_map(map);
  } else {
    map->used = 0;
    memset(map->buckets, 0, map->capacity * sizeof(HashEntry));
  }
}

bool hashmap_entry_valid(HashEntry *e) {
  return e && e->key && e->key != TOMBSTONE;
}
#ifndef HT_KEY_TYPE
# error "define HT_KEY_TYPE before including hashtable.c"
#else

#include "hashtable.h"
#include "heap.h"

#include <string.h>

#define HT_ITEM_TYPE CAT(CAT(item, HT_NAME), t)

typedef struct {
  HT_KEY_TYPE key;
  void *value;
} HT_ITEM_TYPE;

struct HT_STRUCT {
  int size;
  int capacity;
  HT_ITEM_TYPE *table;
  heap_t *heap;
};

HT_TYPE *P(new)(heap_t *heap)
{
  HT_TYPE *ht = heap_malloc(heap, sizeof(HT_TYPE));
  ht->size = 0;
  ht->capacity = 0;
  ht->table = 0;
  ht->heap = heap;
  return ht;
}

void P(insert0)(HT_TYPE *ht, HT_KEY_TYPE key, void *value)
{
  size_t i = HT_HASH(key) % ht->capacity;

  while (1) {
    HT_ITEM_TYPE *item = &ht->table[i];
    if (!item->key || !HT_COMPARE(key, item->key)) {
      item->value = value;
      if (!item->key) {
        /* TODO: free key */
        item->key = HT_DUP(ht->heap, key);
      }
      break;
    }
    i = (i + 1) % ht->capacity;
  }

  ht->size++;
}

void P(resize)(HT_TYPE *ht)
{
  HT_TYPE tmp;
  tmp.size = ht->size;
  tmp.capacity = ht->capacity == 0 ? 128 : ht->capacity * 2;

  size_t table_size = tmp.capacity * sizeof(HT_ITEM_TYPE);
  tmp.table = heap_malloc(ht->heap, table_size);
  memset(tmp.table, 0, table_size);

  for (int i = 0; i < ht->capacity; i++) {
    HT_ITEM_TYPE *item = &ht->table[i];
    if (item->key) {
      P(insert0)(&tmp, item->key, item->value);
    }
  }

  ht->capacity = tmp.capacity;
  heap_free(ht->heap, ht->table);
  ht->table = tmp.table;
}

void P(insert)(HT_TYPE *ht, HT_KEY_TYPE key, void *value)
{
  if (ht->capacity <= (ht->size + 1) * 2) {
    P(resize)(ht);
  }

  P(insert0)(ht, key, value);
}

void *P(get)(HT_TYPE *ht, HT_KEY_TYPE key)
{
  if (ht->size == 0) return NULL;

  size_t i = HT_HASH(key) % ht->capacity;

  while (1) {
    HT_ITEM_TYPE *item = &ht->table[i];
    if (item->key) {
      if (HT_COMPARE(key, item->key)) {
        return item->value;
      }
    }
    else {
      return NULL;
    }
    i = (i + 1) % ht->capacity;
  }
}

void P(del)(HT_TYPE *ht)
{
  heap_free(ht->heap, ht->table);
  heap_free(ht->heap, ht);
}

int P(size)(HT_TYPE *ht)
{
  return ht->size;
}

#endif

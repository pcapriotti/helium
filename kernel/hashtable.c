#include "hashtable.h"
#include "heap.h"

#include <string.h>

//djb2 hash function by Dan Bernstein.
static inline size_t strhash(const char* str)
{
    size_t h = 5381;
    while (*str) h = ((h << 5) + h) ^ *str++;
    return h;
}

typedef struct {
  ht_key_t key;
  void *value;
} item_t;

struct hashtable_struct_t {
  int size;
  int capacity;
  item_t *table;
  heap_t *heap;
};

hashtable_t *ht_new(heap_t *heap)
{
  hashtable_t *ret = heap_malloc(heap, sizeof(hashtable_t));
  ret->size = 0;
  ret->capacity = 0;
  ret->table = 0;
  return ret;
}

void ht_insert0(hashtable_t *ht, ht_key_t key, void *value)
{
  size_t i = strhash(key) % ht->capacity;

  while (1) {
    item_t *item = &ht->table[i];
    if (!item->key || strcmp(key, item->key)) {
      item->value = value;
      if (!item->key) {
        char *key2 = heap_malloc(ht->heap, strlen(key));
        strcpy(key2, key);
        item->key = key2;
      }
      break;
    }
    i = (i + 1) % ht->capacity;
  }

  ht->size++;
}

void ht_resize(hashtable_t *ht)
{
  hashtable_t tmp;
  tmp.size = ht->size;
  tmp.capacity = ht->capacity == 0 ? 128 : ht->capacity * 2;

  size_t table_size = tmp.capacity * sizeof(item_t);
  tmp.table = heap_malloc(ht->heap, table_size);
  memset(tmp.table, 0, table_size);

  for (int i = 0; i < ht->capacity; i++) {
    item_t *item = &ht->table[i];
    if (item->key) {
      ht_insert0(&tmp, item->key, item->value);
    }
  }

  ht->capacity = tmp.capacity;
  heap_free(ht->heap, ht->table);
  ht->table = tmp.table;
}

void ht_insert(hashtable_t *ht, ht_key_t key, void *value)
{
  if (ht->capacity <= (ht->size + 1) * 2) {
    ht_resize(ht);
  }

  ht_insert0(ht, key, value);
}

void *ht_get(hashtable_t *ht, ht_key_t key)
{
  if (ht->size == 0) return NULL;

  size_t i = strhash(key) % ht->capacity;

  while (1) {
    item_t *item = &ht->table[i];
    if (item->key) {
      if (!strcmp(key, item->key)) {
        return item->value;
      }
    }
    else {
      return NULL;
    }
    i = (i + 1) % ht->capacity;
  }
}

void ht_del(hashtable_t *ht)
{
  heap_free(ht->heap, ht->table);
  heap_free(ht->heap, ht);
}

int ht_size(hashtable_t *ht)
{
  return ht->size;
}

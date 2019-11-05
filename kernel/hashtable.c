#include "heap.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* djb2 hash function by Dan Bernstein */
static inline size_t strhash(const char* str)
{
    size_t h = 5381;
    while (*str) h = ((h << 5) + h) ^ *str++;
    return h;
}

static inline char *strdup(heap_t *heap, const char *str)
{
  size_t size = strlen(str) + 1;
  char *ret = heap_malloc(heap, size);
  memcpy(ret, str, size);
  return ret;
}

/* string key */
#define HT_KEY_TYPE const char *
#define HT_NAME string
#define HT_HASH strhash
#define HT_COMPARE(a, b) (!strcmp(a, b))
#define HT_DUP strdup
#include "hashtable.impl.h"
#undef HT_KEY_TYPE
#undef HT_NAME
#undef HT_HASH
#undef HT_COMPARE
#undef HT_DUP

/* 32 bit integer key */
#define HT_KEY_TYPE uint32_t
#define HT_NAME u32
#define HT_HASH(x) ((uint32_t)((x) * 2654435761))
#define HT_COMPARE(a, b) ((a) == (b))
#define HT_DUP(h, x) (x)
#include "hashtable.impl.h"
#undef HT_KEY_TYPE
#undef HT_NAME
#undef HT_HASH
#undef HT_COMPARE
#undef HT_KEYSIZE

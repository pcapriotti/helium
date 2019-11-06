#include "allocator.h"

void *allocator_alloc(allocator_t *a, size_t sz)
{
  return a->alloc(a->data, sz);
}

void allocator_free(allocator_t *a, void *x)
{
  a->free(a->data, x);
}

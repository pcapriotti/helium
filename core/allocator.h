#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stddef.h>

typedef struct allocator {
  void *(*alloc)(void *data, size_t size);
  void *(*free)(void *data, void *x);
  void *data;
} allocator_t;

void *allocator_alloc(allocator_t *a, size_t sz);
void allocator_free(allocator_t *a, void *x);

#endif /* ALLOCATOR_H */

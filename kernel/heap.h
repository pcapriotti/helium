#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>

struct heap;
typedef struct heap heap_t;
struct frames;

heap_t *heap_new(struct frames *frames);
heap_t *heap_new_with_growth(struct frames *frames, int page_growth);
void *heap_malloc(heap_t *heap, size_t bytes);
void heap_free(heap_t *heap, void *address);
void heap_print_diagnostics(heap_t *heap);

struct allocator;
extern struct allocator heap_allocator;

#endif /* HEAP_H */

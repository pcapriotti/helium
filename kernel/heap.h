#ifndef HEAP_H
#define HEAP_H

#define ROUND(a, i) (((uint32_t)a + (1 << (i)) - 1) >> i)

struct heap;
typedef struct heap heap_t;
struct frames;

heap_t *heap_new(struct frames *frames);
void *heap_malloc(heap_t *heap, size_t bytes);
void heap_free(heap_t *heap, void *address);
void heap_print_diagnostics(heap_t *heap);

#endif /* HEAP_H */

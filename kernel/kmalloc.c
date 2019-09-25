#include "kmalloc.h"
#include "heap.h"

#include <stdint.h>

/* linked list of free blocks, in increasing order
   of address */
static heap_t *kernel_heap;

int kmalloc_init(struct frames *frames)
{
  kernel_heap = heap_new(frames);
  return kernel_heap != 0;
}

void *kmalloc(size_t bytes)
{
  return heap_malloc(kernel_heap, bytes);
}

void kfree(void *address)
{
  heap_free(kernel_heap, address);
}

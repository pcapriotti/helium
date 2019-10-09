#include "heap.h"
#include "kmalloc.h"
#include "memory.h"

#include <stdint.h>

/* linked list of free blocks, in increasing order
   of address */
static heap_t *kernel_heap;

int kmalloc_init()
{
  kernel_heap = heap_new(&kernel_frames);
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

#include "buddy/frames.h"
#include "heap.h"
#include "memory.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#define KMALLOC_DEBUG 0

/* simplistic allocator design, with just one list of free blocks */

struct block;

typedef struct block {
  uint32_t size;
  union {
    struct {
      struct block *next;
      struct block *prev;
    } links;
    unsigned char memory[0];
  } data;
} block_t;

#define MIN_ALLOC_SIZE sizeof(block_t)
#define PAGE_GROWTH 16

struct heap {
  /* linked list of free blocks, in increasing order
     of address */
  block_t *free_blocks;
  frames_t *frames;
};

heap_t *heap_new(frames_t *frames)
{
  /* get a block from the frame allocator */
  uint32_t size = PAGE_GROWTH << PAGE_BITS;
  assert(size >= sizeof(heap_t));
  void *block = frames_alloc(frames, size);
  if (!block) return 0;

  /* reserve space for the heap data structures */
  heap_t *heap = block;
  heap->free_blocks = (block_t *)(heap + 1);

  heap->free_blocks->size = size - sizeof(heap_t);
  heap->free_blocks->data.links.prev = 0;
  heap->free_blocks->data.links.next = 0;
  heap->frames = frames;
  return heap;
}

void *heap_malloc(heap_t *heap, size_t bytes)
{
#if KMALLOC_DEBUG
  kprintf("kmalloc(%u)\n", bytes);
#endif
  if (bytes < MIN_ALLOC_SIZE) bytes = MIN_ALLOC_SIZE;
  block_t *b = heap->free_blocks;
  while (b) {
    if (b->size >= bytes + sizeof(block_t) + MIN_ALLOC_SIZE) {
#if KMALLOC_DEBUG
      kprintf("  splitting block of size %u\n", b->size);
#endif
      /* split */
      void *mem = b->data.memory;
      block_t *b1 = mem + bytes;
      if (b->data.links.prev) {
        b->data.links.prev->data.links.next = b1;
      } else {
        heap->free_blocks = b1;
      }
      b1->size = b->size - ((void*)b1 - (void*)b);
      b1->data.links.prev = b->data.links.prev;
      b1->data.links.next = b->data.links.next;
      if (b1->data.links.next)
        b1->data.links.next->data.links.prev = b1;
      b->size = bytes;
      return mem;
    } else if (b->size >= bytes) {
#if KMALLOC_DEBUG
      kprintf("  taking whole block of size %u\n", b->size);
#endif
      /* just take the whole block */
      block_t *prev = b->data.links.prev;
      block_t *next = b->data.links.next;
      if (prev)
        prev->data.links.next = b->data.links.next;
      else
        heap->free_blocks = b->data.links.next;

      if (next)
        next->data.links.prev = b->data.links.prev;
      return b->data.memory;
    }
    if (!b->data.links.next) {
#if KMALLOC_DEBUG
      kprintf("  no more blocks, requesting a new one\n");
#endif
      /* request more memory */
      int num_pages = ROUND(bytes, PAGE_BITS) + 1;
      if (num_pages < PAGE_GROWTH) num_pages = PAGE_GROWTH;
      unsigned long size = num_pages << PAGE_BITS;
      block_t *block = frames_alloc(heap->frames, size);
      b->data.links.next = block;
      if (block) {
        block->size = size;
        block->data.links.prev = b;
        block->data.links.next = 0;
#if KMALLOC_DEBUG
        kprintf("  got block of size %u\n", block->size);
#endif
      }
      b = block;
    } else {
      b = b->data.links.next;
    }
  }
  return 0;
}

void heap_free(heap_t *heap, void *address)
{
  block_t *block = address - sizeof(uint32_t);
  if (block < heap->free_blocks) {
    block->data.links.next = heap->free_blocks;
    block->data.links.prev = 0;
    heap->free_blocks->data.links.prev = block;
    heap->free_blocks = block;
    return;
  }
  block_t *b = heap->free_blocks;
  while (b && b < block) {
    block_t *next = b->data.links.next;
    if (next >= block) {
      b->data.links.next = block;
      block->data.links.prev = b;
      block->data.links.next = next;
      if (next)
        next->data.links.prev = block;
      return;
    }
    else {
      b = next;
    }
  }
  if (b) {
    b->data.links.next = block;
    block->data.links.prev = b;
    block->data.links.next = 0;
  } else {
    heap->free_blocks = block;
    block->data.links.prev = 0;
    block->data.links.next = 0;
  }
}

void heap_print_diagnostics(heap_t *heap)
{
  block_t *b = heap->free_blocks;
  while (b) {
    kprintf("block size: %d at %#x\n");
    b = b->data.links.next;
  }
}
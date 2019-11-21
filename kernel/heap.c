#include "core/allocator.h"
#include "core/debug.h"
#include "core/util.h"
#include "frames.h"
#include "heap.h"
#include "memory.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#define KMALLOC_DEBUG 0
#define KMALLOC_UNIT 4

#ifndef _HELIUM
# include <stdio.h>
# define serial_printf printf
#endif

/* simplistic allocator design, with just one list of free blocks */

struct block;

typedef struct block {
  uint32_t size;
  union {
    struct {
      struct block *next;
      struct block *prev;
    };
    unsigned char memory[0];
  };
} block_t;

#define MIN_ALLOC_SIZE sizeof(block_t)
#define DEFAULT_PAGE_GROWTH 16

struct heap {
  /* linked list of free blocks, in increasing order
     of address */
  block_t *free_blocks;
  frames_t *frames;
  int page_growth;
};

heap_t *heap_new(frames_t *frames)
{
  return heap_new_with_growth(frames, DEFAULT_PAGE_GROWTH);
}

heap_t *heap_new_with_growth(frames_t *frames, int page_growth)
{
  /* get a block from the frame allocator */
  uint32_t size = page_growth << PAGE_BITS;
  assert(size >= sizeof(heap_t));
  uint64_t frame = frames_alloc(frames, size);
#ifdef _HELIUM
  assert(frame < KERNEL_MEMORY_END);
#endif
  void *block = (void *) (size_t) frame;
  assert((size_t) block % KMALLOC_UNIT == 0);
  if (!block) return 0;

  /* reserve space for the heap data structures */
  heap_t *heap = block;
  heap->free_blocks = (block_t *)(heap + 1);

  heap->free_blocks->size = size - sizeof(heap_t);
  heap->free_blocks->prev = 0;
  heap->free_blocks->next = 0;
  heap->frames = frames;
  heap->page_growth = page_growth;
  return heap;
}

void *heap_malloc(heap_t *heap, size_t bytes)
{
#if KMALLOC_DEBUG
  serial_printf("heap_malloc(%lu) heap: %p\n", bytes, heap);
#endif

  /* only allocate multiples of KMALLOC_UNIT bytes */
  bytes += KMALLOC_UNIT - 1;
  bytes = bytes - (bytes % KMALLOC_UNIT);

  if (bytes < MIN_ALLOC_SIZE) bytes = MIN_ALLOC_SIZE;
  block_t *b = heap->free_blocks;
  while (b) {
    if (b->size >= bytes + sizeof(block_t) + MIN_ALLOC_SIZE) {
#if KMALLOC_DEBUG
      serial_printf("  splitting block of size %u\n", b->size);
#endif
      /* split */
      void *mem = b->memory;
      block_t *b1 = mem + bytes;
      assert((size_t) b1 % KMALLOC_UNIT == 0);
      if (b->prev) {
        b->prev->next = b1;
      } else {
        heap->free_blocks = b1;
      }
      b1->size = b->size - ((void*)b1 - (void*)b);
      b1->prev = b->prev;
      b1->next = b->next;
      if (b1->next)
        b1->next->prev = b1;
      b->size = bytes;
      return mem;
    } else if (b->size >= bytes) {
#if KMALLOC_DEBUG
      serial_printf("  taking whole block of size %u\n", b->size);
#endif
      /* just take the whole block */
      block_t *prev = b->prev;
      block_t *next = b->next;
      if (prev)
        prev->next = b->next;
      else
        heap->free_blocks = b->next;

      if (next)
        next->prev = b->prev;
      return b->memory;
    }
    if (!b->next) {
#if KMALLOC_DEBUG
      serial_printf("  no more blocks, requesting a new one\n");
#endif
      /* request more memory */
      int num_pages = DIV_UP(bytes, 1 << PAGE_BITS);
      if (num_pages < heap->page_growth) num_pages = heap->page_growth;
      unsigned long size = num_pages << PAGE_BITS;
      uint64_t frame = frames_alloc(heap->frames, size);
#if _HELIUM
      assert(frame < KERNEL_MEMORY_END);
#endif
      block_t *block = (void *) (size_t) frame;
      b->next = block;
      if (block) {
        block->size = size;
        block->prev = b;
        block->next = 0;
#if KMALLOC_DEBUG
        serial_printf("  got block of size %u\n", block->size);
#endif
      }
      b = block;
    } else {
      b = b->next;
    }
  }
  return 0;
}

void heap_free(heap_t *heap, void *address)
{
#if KMALLOC_DEBUG
  serial_printf("heap_free(%p) heap: %p\n", address, heap);
#endif

  /* freeing a null pointer does nothing */
  if (!address) return;

  block_t *block = address - sizeof(uint32_t);
  if (block < heap->free_blocks) {
    block->next = heap->free_blocks;
    block->prev = 0;
    heap->free_blocks->prev = block;
    heap->free_blocks = block;
    return;
  }
  block_t *b = heap->free_blocks;
  while (b && b < block) {
    block_t *next = b->next;
    if (next >= block) {
      b->next = block;
      block->prev = b;
      block->next = next;
      if (next)
        next->prev = block;
      return;
    }
    else {
      b = next;
    }
  }
  if (b) {
    b->next = block;
    block->prev = b;
    block->next = 0;
  } else {
    heap->free_blocks = block;
    block->prev = 0;
    block->next = 0;
  }
}

void heap_print_diagnostics(heap_t *heap)
{
  block_t *b = heap->free_blocks;
  while (b) {
    serial_printf("block size: %d at %p\n", b->size, b);
    b = b->next;
  }
}

static void *heap_allocator_alloc(void *data, size_t size)
{
  heap_t *heap = data;
  return heap_malloc(heap, size);
}

static void heap_allocator_free(void *data, void *x)
{
  heap_t *heap = data;
  return heap_free(heap, x);
}

allocator_t heap_allocator = {
  .alloc = heap_allocator_alloc,
  .free = heap_allocator_free,
};

#include "buddy/frames.h"
#include "core/debug.h"
#include "kmalloc.h"
#include "memory.h"

#include <stdint.h>

#define KMALLOC_DEBUG 0

/* simplistic allocator design, with just one list of free blocks */

struct block_struct;

typedef struct {
  struct block_struct *next;
  struct block_struct *prev;
} block_data_t;
typedef struct block_struct block_t;

struct block_struct {
  uint32_t size;
  union {
    struct {
      block_t *next;
      block_t *prev;
    } links;
    unsigned char memory[0];
  } data;
};

#define MIN_ALLOC_SIZE sizeof(block_t)
#define PAGE_GROWTH 16

/* linked list of free blocks, in increasing order
   of address */
static block_t *free_blocks;
static frames_t *frames;

int kmalloc_init(frames_t *_frames)
{
  frames = _frames;

  uint32_t size = PAGE_GROWTH << PAGE_BITS;
  free_blocks = frames_alloc(frames, size);
  if (!free_blocks) return -1;
  free_blocks->size = size;
  free_blocks->data.links.prev = 0;
  free_blocks->data.links.next = 0;
  return 0;
}

void *kmalloc(size_t bytes)
{
#if KMALLOC_DEBUG
  kprintf("kmalloc(%u)\n", bytes);
#endif
  if (bytes < MIN_ALLOC_SIZE) bytes = MIN_ALLOC_SIZE;
  block_t *b = free_blocks;
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
        free_blocks = b1;
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
        free_blocks = b->data.links.next;

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
      block_t *block = frames_alloc(frames, size);
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

void kfree(void *address)
{
  block_t *block = address - sizeof(uint32_t);
  if (block < free_blocks) {
    block->data.links.next = free_blocks;
    block->data.links.prev = 0;
    free_blocks->data.links.prev = block;
    free_blocks = block;
    return;
  }
  block_t *b = free_blocks;
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
    free_blocks = block;
    block->data.links.prev = 0;
    block->data.links.next = 0;
  }
}

void kmalloc_print_diagnostics()
{
  block_t *b = free_blocks;
  while (b) {
    kprintf("block size: %d at %#x\n");
    b = b->data.links.next;
  }
}

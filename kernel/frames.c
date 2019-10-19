#include "frames.h"
#if _HELIUM
#include "paging/paging.h"
#endif

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#define FRAMES_DEBUG 0

#if _HELIUM
#include "core/debug.h"
#define FRAMES_PANIC(ret, ...) do { \
  kprintf("[frames] "); \
  kprintf(__VA_ARGS__); \
  return ret; } while(0)
#else
#include <stdio.h>
#define FRAMES_PANIC(ret, ...) do { \
  fprintf(stderr, "[frames] "); \
  fprintf(stderr, __VA_ARGS__); \
  return ret; } while(0)
#endif

#if !_HELIUM
#define kprintf printf
#define serial_printf printf
#endif

#if FRAMES_DEBUG
# define TRACE(...) serial_printf(__VA_ARGS__)
#else
# define TRACE(...) do {} while(0)
#endif

#if FRAMES_DEBUG
#define DIAGNOSTICS(frames) \
  TRACE("----\n"); \
  frames_dump_diagnostics(frames); \
  TRACE("----\n");
#else
#define DIAGNOSTICS(frames) do {} while (0)
#endif

/* operations on bitvectors defined as arrays of uint32_t */
#define GET_BIT(v, index) \
  ((v)[(index) >> 5] & (1 << ((index) & 0x1f)))
#define SET_BIT(v, index) \
  ((v)[(index) >> 5] |= (1 << ((index) & 0x1f)))
#define UNSET_BIT(v, index) \
  ((v)[(index) >> 5] &= ~(1 << ((index) & 0x1f)))
#define FLIP_BIT(v, index) \
  ((v)[(index) >> 5] ^= (1 << ((index) & 0x1f)))

typedef struct block_t {
  /* physical pointer to next block */
  uint64_t next;
  /* physica address of this block */
  uint64_t current;
  /* physical pointer to previous block */
  uint64_t prev;
} block_t;

/* temporarely map blocks that don't resize in the identity mapped
   portion of RAM */
static inline block_t *map_block(uint64_t p)
{
#if _HELIUM
  if (p < KERNEL_MEMORY_END) {
    return (block_t *)(uint32_t) p;
  }
  else {
    block_t *block = paging_temp_map_page(p);
    return block;
  }
#else
  return (block_t *)p;
#endif
}

static inline void unmap_block(block_t *block)
{
#if _HELIUM
  if (block && block->current >= KERNEL_MEMORY_END) {
    paging_temp_unmap_page(block);
  }
#endif
}

/*
A standard buddy memory allocator. This manages a contiguous chunk of
memory of size a power of two. The memory is divided into blocks of
size also a power of two, and arranged as follows:

_______________
_______________
___0___|___1___
_2_|_3_|_4_|_5_
6|7|8|9|a|b|c|d
      ...

Where each block, except the top one, is half of its parent block on
the previous row. The two halves of a block are called *buddies*. Note
that the unique block of maximum order is not numbered. This is
for two reasons:

 - it does not have a buddy
 - with this convention, taking the buddy of a block just corresponds
   to flipping the lowest bit of its index.

The numbers in the table above are referred to as block *indices*,
while the index of a block within its row is its *relative index*. The
order of a block is the base-two logarithm of its size. The various
parameters of the system are:

 - start: pointer to the beginning of the managed memory chunk
 - end: pointer past the end of managed memory
 - min_order: order of the smallest blocks
 - max_order: order of the largest block

The state of the system is maintained in two data structures:

 - an array of (intrusive) doubly-linked lists, one for every order,
   containing the available blocks;
 - a bit vector with one bit for every block, called the metadata table.

Block can be *allocated*, *available* or neither. The
metadata bit for a block is set exactly when some descendent of
the block is allocated.

The first invariant that the system maintains is that the available blocks,
plus the allocated blocks, form a partition of the top block. This
makes sure that every byte in the initial memory chunk is only used by
one allocated block at a time, and that all the memory is potentially
allocatable.

The second invariant is that if a block is available, then its buddy
is not. This is to prevent fragmentation, by making sure that
consecutive blocks tend to be merged into larger blocks.

There are essentially two operations in the system: allocation and
deallocation. Allocation creates a new allocated block of the given
order, while deallocation destroys it.

Allocation works quite simply: we first check the linked list of the
given order to see whether it contains an available block; if so, we
turn it into an allocated block and return it. Otherwise, we
recursively allocate a block of larger order, make half available and
the other allocated, and return the allocated one. In either case, we
set the metadata bit of the returned block.

Deallocation is more complicated. First, it needs to determine the
order of the block being deallocated, given just its offset. Every
block has a maximum order it can have, determined by the alignment of
its offset. The correct order is then the maximum one such that all
the blocks with that offset of smaller orders are *not* allocated.

Now observe that if the buddy of an allocated block is not available,
then its metadata bit must be set (because no containing block can be
allocated, as allocated blocks are disjoint). It follows that we can
check the availability of the buddy block just by testing its
bit. This allows us to maintain the second invariant: if both the
block and its buddy would end up being available, we merge them by
making them unavailable and recursing on the parent block. */

static inline unsigned int frames_abs_index(frames_t *frames,
                                            unsigned int rel_index,
                                            unsigned int order)
{
  return rel_index + (1ULL << (frames->max_order - order)) - 2;
}

static inline unsigned int frames_rel_index(frames_t *frames,
                                            unsigned int index,
                                            unsigned int order)
{
  return index + 2 - (1ULL << (frames->max_order - order));
}

static inline unsigned int frames_block_index(frames_t *frames,
                                              uint64_t block,
                                              unsigned int order)
{
  uint64_t offset = block - frames->start;
  /* TRACE("offset = %" PRIx64 "\n", offset); */
  /* TRACE("rel_index = %u\n", (unsigned int) (offset >> order)); */
  unsigned int index = frames_abs_index(frames, offset >> order, order);
  /* TRACE("index = %u\n", index); */
  /* TRACE("limit = %u\n", (1U << (frames->max_order - frames->min_order + 1))); */
  assert(index < (1UL << (frames->max_order - frames->min_order + 1)));
  return index;
}

static inline uint64_t frames_index_block(frames_t *frames, unsigned int index, unsigned int order)
{
  uint64_t i = frames_rel_index(frames, index, order);
  uint64_t offset = i << order;
  return frames->start + offset;
}

static inline uint64_t *block_head(frames_t *frames, unsigned int order)
{
  return &frames->free[order - frames->min_order];
}

static inline void list_add(uint64_t *list, block_t *block)
{
  block->prev = 0;
  block->next = 0;

  if (*list) {
    block_t *head = map_block(*list);
    block->next = head->current;
    head->prev = block->current;
    unmap_block(head);
  }

  *list = block->current;
}

static inline void list_remove(uint64_t *list, block_t *block)
{
  uint64_t prev = block->prev;
  uint64_t next = block->next;

  block_t *nextb = map_block(next);
  block_t *prevb = map_block(prev);

  if (next) nextb->prev = prev;
  if (prev) prevb->next = next;
  if (!prev) *list = next;

  unmap_block(nextb);
  unmap_block(prevb);
}

static inline uint64_t list_take(uint64_t *list)
{
  if (*list) {
    uint64_t ret = *list;
    block_t *block = map_block(ret);
    if (block->next) {
      block_t *next = map_block(block->next);
      next->prev = 0;
      unmap_block(next);
    }
    *list = block->next;
    unmap_block(block);
    return ret;
  }

  return 0;
}

static void frames_lock(frames_t *frames)
{
  if (frames->lock)
    frames->lock(frames);
}

static void frames_unlock(frames_t *frames)
{
  if (frames->unlock)
    frames->unlock(frames);
}

static void add_blocks(unsigned int order, uint64_t start, frames_t *frames,
                       int (*mem_info)(uint64_t start, uint64_t size, void *data),
                       void *data)
{
  int info = mem_info(start, 1ULL << order, data);
  TRACE("block %#" PRIx64 " order %u info %d\n", start, order, info);

  /* if the block is usable, just add it to the list */
  if (info == MEM_INFO_USABLE) {
    block_t *block = map_block(start);
    block->current = start;
    TRACE("adding block %#" PRIx64 " order %u\n", start, order);
    list_add(block_head(frames, order), block);
    unmap_block(block);
    return;
  }

  /* if it is partially usable, split it and recurse */
  if (order > frames->min_order && info == MEM_INFO_PARTIALLY_USABLE) {
    add_blocks(order - 1, start, frames, mem_info, data);
    add_blocks(order - 1, start + (1ULL << (order - 1)), frames, mem_info, data);
  }
}

static void mark_blocks(frames_t *frames, uint64_t start, unsigned int order,
                        int (*mem_info)(uint64_t start, uint64_t size, void *data),
                        void *data)
{
  int info = mem_info(start, 1ULL << order, data);
  /* TRACE("mark block %#lx order %u info %d\n", */
  /*       start, order, info); */
  if (info == MEM_INFO_USABLE) return;

  if (order < frames->max_order) {
    unsigned int index = frames_block_index(frames, start, order);
    SET_BIT(frames->metadata, index);
    /* TRACE("set bit for block %#lx order %u index %d\n", */
    /*       start, order, index); */
  }

  if (order > frames->min_order && info == MEM_INFO_PARTIALLY_USABLE) {
    mark_blocks(frames, start, order - 1, mem_info, data);
    mark_blocks(frames, start + (1ULL << (order - 1)), order - 1, mem_info, data);
  }
}

/* Create an allocated block of the given order.

   If the metadata is invalid, this function can still be used, but it
   does not update the metadata. */
uint64_t take_block(frames_t *frames, unsigned int order)
{
  if (order > frames->max_order) return 0;
  if (order <= frames->min_order) order = frames->min_order;

  uint64_t *head = block_head(frames, order);
  /* if there is an available block of the correct order, take it */
  uint64_t frame = list_take(head);
  if (!frame) {
    /* otherwise, split the next one up */
    uint64_t frame1 = take_block(frames, order + 1);
    if (!frame1) return 0;
    uint64_t frame2 = frame1 + (1ULL << order);
    block_t *block2 = map_block(frame2);
    block2->current = frame2;
    TRACE("  adding split block %" PRIx64 " order %u\n",
          frame2, order);
    list_add(head, block2);
    unmap_block(block2);
    frame = frame1;
  }

  assert(frame >= frames->start && frame < frames->end);
  if (frames->metadata && order < frames->max_order) {
    TRACE("set bit order %u index 0x%x\n", order,
           frames_block_index(frames, frame, order));
    SET_BIT(frames->metadata, frames_block_index(frames, frame, order));
  }
  return frame;
}

/* Create a new buddy allocator for a chunk of memory.

  start - pointer to the beginning of the memory chunk
  min_order, max_order - block parameters (see comment above)
  mem_info, data - closure that returns whether a fragment of the
    memory chunk is available for use

  First, the available blocks are constructed and added to the lists
  using recursive invocations of mem_info.

  At this point, metadata needs to be created. Unfortunately, because
  there is no metadata yet, when we allocate it no metadata changes
  can be recorded.  So after the metadata block has been allocated, we
  first synchronise it with the initial block setup, then reconstruct
  the metadata changes that would have happened during its allocation. */
int frames_init(frames_t *frames, frames_t *aux_frames,
                uint64_t start, uint64_t end,
                unsigned int min_order,
                int (*mem_info)(uint64_t start, uint64_t size, void *data),
                void *data)
{
  if (min_order < ORDER_OF(sizeof(block_t))) {
    FRAMES_PANIC(-1, "min_order must be at least %u\n", ORDER_OF(sizeof(block_t)));
  }

  frames->start = start;
  frames->end = end;
  frames->min_order = min_order;
  frames->max_order = ORDER64_OF(end - start);
  frames->lock = 0;
  frames->unlock = 0;
  if (frames->max_order < frames->min_order) {
    FRAMES_PANIC(-1, "min_order too large\n");
  }
  for (unsigned int k = frames->min_order; k <= frames->max_order; k++) {
    *block_head(frames, k) = 0;
  }

  /* add all blocks */
  add_blocks(frames->max_order, start, frames, mem_info, data);

  DIAGNOSTICS(frames);

  /* set metadata */
  frames->metadata = 0;
  unsigned int meta_order = frames->max_order - frames->min_order - 2;
  if (meta_order <= 2) meta_order = 2;
  if (meta_order < frames->min_order) meta_order = frames->min_order;
  uint64_t metadata_frame = 0;

  if (aux_frames) {
    metadata_frame = frames_alloc(aux_frames, 1ULL << meta_order);
  }
  else {
    metadata_frame = take_block(frames, meta_order);
  }

  if (!metadata_frame) FRAMES_PANIC(-1, "not enough memory for frame metadata\n");
#if _HELIUM
  assert(metadata_frame < KERNEL_MEMORY_END);
#endif
  frames->metadata = (uint32_t *) (size_t) metadata_frame;


  TRACE("setting initial metadata\n");

  memset(frames->metadata, 0, 1ULL << meta_order);
  /* synchronise metadata with initial blocks */
  mark_blocks(frames, start, frames->max_order, mem_info, data);

  TRACE("  done\n");

  /* execute metadata changes for the metadata block allocation */
  if (!aux_frames) {
    TRACE("replaying metadata for %p order %u\n",
          frames->metadata, meta_order);
    int index = frames_block_index(frames, metadata_frame, meta_order);
    while (index > 0) {
      TRACE("setting bit for index %d\n", index);
      SET_BIT(frames->metadata, index);
      index = (index >> 1) - 1;
    }
  }

  DIAGNOSTICS(frames);
  return 0;
}

uint64_t frames_available_memory(frames_t *frames)
{
  uint64_t total = 0;
  for (unsigned int k = frames->min_order; k <= frames->max_order; k++) {
    uint64_t size = 1ULL << k;
    uint64_t frame = *block_head(frames, k);
    while (frame) {
      total += size;
      block_t *block = map_block(frame);
      frame = block->next;
      unmap_block(block);
    }
  }
  return total;
}

uint64_t frames_alloc(frames_t *frames, size_t sz)
{
  unsigned int order = ORDER_OF(sz);
  if (order < frames->min_order) order = frames->min_order;

  frames_lock(frames);
  uint64_t ret = take_block(frames, order);
  frames_unlock(frames);

  TRACE("allocated %#" PRIx64 " size 0x%lx (order %u)\n", ret, sz, order);
  DIAGNOSTICS(frames);
  return ret;
}

void frames_dump_diagnostics(frames_t *frames)
{
  frames_lock(frames);
  for (unsigned int k = frames->min_order; k <= frames->max_order; k++) {
    uint64_t frame = *block_head(frames, k);
    int nonempty = frame != 0;
    if (nonempty) kprintf("order %d: ", k);
    while (frame) {
      kprintf("%#" PRIx64 " ", frame);

      block_t *block = map_block(frame);
      frame = block->next;
      unmap_block(block);
    }
    if (nonempty) kprintf("\n");
  }
  frames_unlock(frames);
  kprintf("total memory: %" PRIu64 " B\n  from %#" PRIx64 " to %#" PRIx64 "\n",
          frames->end - frames->start,
          frames->start, frames->end);
  kprintf("free memory:  %" PRIu64 " B\n", frames_available_memory(frames));
}

void frames_free_order(frames_t *frames, uint64_t p, unsigned int order)
{
  block_t *block = map_block(p);
  block->current = p;

  /* find buddy */
  block_t *buddy = 0;
  if (order < frames->max_order) {
    int index = frames_block_index(frames, block->current, order);
    UNSET_BIT(frames->metadata, index);
    TRACE("checking buddy for 0x%lx order %d index 0x%x\n",
           p, order, index);
    if (!GET_BIT(frames->metadata, index ^ 1)) {
      uint64_t buddy_addr = frames_index_block(frames, index ^ 1, order);
      TRACE("  buddy_addr = %#" PRIx64 "\n", buddy_addr);
      buddy = map_block(buddy_addr);
      TRACE("  buddy current = %#" PRIx64 "\n", buddy->current);
    }
  }

  /* merge */
  if (buddy) {
    TRACE("  buddy found: %#" PRIx64 "\n", buddy->current);
    list_remove(block_head(frames, order), buddy);
    if (buddy->current < block->current) block = buddy;
    frames_free_order(frames, block->current, order + 1);

    unmap_block(buddy);
    unmap_block(block);
    return;
  }
  else {
    list_add(block_head(frames, order), block);
    unmap_block(block);
  }

  TRACE("freed 0x%" PRIx64 " of order %d\n", p, order);
  DIAGNOSTICS(frames);
}

unsigned int frames_find_order(frames_t *frames, uint64_t p)
{
  unsigned int order = __builtin_ctzl(p - frames->start);
  TRACE("maximum order for %#" PRIx64 ": %u\n", p, order);

  for (unsigned int k = order - 1; k >= frames->min_order; k--) {
    unsigned int i = frames_block_index(frames, p, k);
    TRACE("  order %u index 0x%x: %s\n", k, i, GET_BIT(frames->metadata, i) ? "yes" : "no");
  }
  TRACE("  ---\n");

  unsigned int index = frames_block_index(frames, p, order);
  for (; order > frames->min_order; order--) {
    index = (index << 1) + 2; /* first child */
    TRACE("  bit order %u index 0x%x: %s\n",
           order - 1, index,
           GET_BIT(frames->metadata, index) ? "yes" : "no");
    if (!GET_BIT(frames->metadata, index)) break;
  }
  TRACE("  order %d\n", order);
  return order;
}

void frames_free(frames_t *frames, uint64_t p)
{
  frames_lock(frames);
  frames_free_order(frames, p, frames_find_order(frames, p));
  frames_unlock(frames);
}

int default_mem_info(uint64_t start, uint64_t size, void *data)
{
  return MEM_INFO_USABLE;
}

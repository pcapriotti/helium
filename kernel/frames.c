#include "frames.h"

#include <assert.h>
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

#if FRAMES_DEBUG
# if _HELIUM
#  define TRACE(...) kprintf(__VA_ARGS__)
# else
#  define TRACE(...) printf(__VA_ARGS__)
# endif
#else
# define TRACE(...) do {} while(0)
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
  /* pointer to next block */
  struct block_t *next;
  /* pointer to previous block */
  struct block_t *prev;
} block_t;

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
struct frames {
  void *start;
  struct block_t *free[MAX_ORDER];
  unsigned int min_order, max_order;

  uint32_t *metadata;
};

static inline unsigned int frames_abs_index(frames_t *frames, unsigned int rel_index, unsigned int order)
{
  return rel_index + (1 << (frames->max_order - order)) - 2;
}

static inline unsigned int frames_rel_index(frames_t *frames, unsigned int index, unsigned int order)
{
  return index + 2 - (1 << (frames->max_order - order));
}

static inline unsigned int frames_block_index(frames_t *frames, void *block, unsigned int order)
{
  uint32_t offset = block - frames->start;
  unsigned int index = frames_abs_index(frames, offset >> order, order);
  assert(index < (1UL << (frames->max_order - frames->min_order + 1)));
  return index;
}

static inline void *frames_index_block(frames_t *frames, unsigned int index, unsigned int order)
{
  return frames->start + (frames_rel_index(frames, index, order) << order);
}

static inline block_t **block_head(frames_t *frames, unsigned int order)
{
  return &frames->free[order - frames->min_order];
}

static inline void list_add(block_t **list, block_t *block)
{
  block->prev = 0;
  block->next = *list;
  if (*list) (*list)->prev = block;
  *list = block;
}

static inline void list_remove(block_t **list, block_t *block)
{
  block_t *prev = block->prev;
  block_t *next = block->next;
  if (next) next->prev = prev;
  if (prev)
    prev->next = next;
  else
    *list = next;
}

static inline block_t *list_take(block_t **list)
{
  if (*list) {
    block_t *ret = *list;
    if ((*list)->next) (*list)->next->prev = 0;
    *list = (*list)->next;
    return ret;
  }

  return 0;
}

void add_blocks(unsigned int order, void *start, frames_t *frames,
                int (*mem_info)(void *start, size_t size, void *data),
                void *data)
{
  int info = mem_info(start, order == 32 ? 0 : 1 << order, data);
  /* TRACE("block %p order %u info %d\n", start, order, info); */

  /* if the block is usable, just add it to the list */
  if (info == MEM_INFO_USABLE) {
    block_t *block = (block_t *)start;
    /* TRACE("adding block %p order %u\n", block, order); */
    list_add(block_head(frames, order), block);
    return;
  }

  /* if it is partially usable, split it and recurse */
  if (order > frames->min_order && info == MEM_INFO_PARTIALLY_USABLE) {
    add_blocks(order - 1, start, frames, mem_info, data);
    add_blocks(order - 1, start + (1 << (order - 1)), frames, mem_info, data);
  }
}

void mark_blocks(frames_t *frames, void *start, unsigned int order,
                 int (*mem_info)(void *start, size_t size, void *data),
                 void *data)
{
  int info = mem_info(start, order == 32 ? 0 : 1 << order, data);
  TRACE("mark block %#lx order %u info %d\n",
        start - frames->start, order, info);
  if (info == MEM_INFO_USABLE) return;

  if (order < frames->max_order) {
    unsigned int index = frames_block_index(frames, start, order);
    SET_BIT(frames->metadata, index);
    TRACE("set bit for block %#lx order %u index %d\n",
          start - frames->start, order, index);
  }

  if (order > frames->min_order && info == MEM_INFO_PARTIALLY_USABLE) {
    mark_blocks(frames, start, order - 1, mem_info, data);
    mark_blocks(frames, start + (1 << (order - 1)), order - 1, mem_info, data);
  }
}

/* Create an allocated block of the given order.

   If the metadata is invalid, this function can still be used, but it
   does not update the metadata. */
void *take_block(frames_t *frames, unsigned int order)
{
  if (order > frames->max_order) return 0;
  if (order <= frames->min_order) order = frames->min_order;

  block_t **head = block_head(frames, order);
  /* if there is an available block of the correct order, take it */
  block_t *block = list_take(head);
  if (!block) {
    /* otherwise, split the next one up */
    void *block1 = take_block(frames, order + 1);
    if (!block1) return 0;
    block_t *block2 = (block_t *)(block1 + (1 << order));
    list_add(head, block2);
    block = block1;
  }

  assert((void *)block >= frames->start);
  if (frames->metadata && order < frames->max_order) {
    TRACE("set bit order %u index 0x%x\n", order,
           frames_block_index(frames, block, order));
    SET_BIT(frames->metadata, frames_block_index(frames, block, order));
  }
  return block;
}

/* Create a new buddy allocator for a chunk of memory.

  start - pointer to the beginning of the memory chunk
  min_order, max_order - block parameters (see comment above)
  mem_info, data - closure that returns whether a fragment of the
    memory chunk is available for use

  Initialisation happens in several phases. First, a frames_t
  structure is constructed on the stack and initialised with no
  available blocks.

  Then, the available blocks are constructed and added to the lists
  using recursive invocations of mem_info.

  At this point, metadata needs to be created. Unfortunately, because
  there is no metadata yet, when we allocate it no metadata changes
  can be recorded.  So after the metadata block has been allocated, we
  first synchronise it with the initial block setup, then reconstruct
  the metadata changes that would have happened during its allocation.

  Finally we allocate a block for the frames_t structure itself, copy
  it over, and return its address. */
frames_t *frames_new(void *start,
                     unsigned int min_order,
                     unsigned int max_order,
                     int (*mem_info)(void *start, size_t size, void *data),
                     void *data)
{
  if (min_order < ORDER_OF(sizeof(block_t))) {
    FRAMES_PANIC(0, "min_order must be at least %lu\n", ORDER_OF(sizeof(block_t)));
  }

  frames_t frames;
  frames.start = start;
  frames.min_order = min_order;
  frames.max_order = max_order;
  if (frames.max_order < frames.min_order) {
    FRAMES_PANIC(0, "min_order too large\n");
  }
  for (unsigned int k = frames.min_order; k <= frames.max_order; k++) {
    *block_head(&frames, k) = 0;
  }

  /* add all blocks */
  add_blocks(frames.max_order, start, &frames, mem_info, data);

  frames_dump_diagnostics(&frames);

  /* set metadata */
  frames.metadata = 0;
  unsigned int meta_order = frames.max_order - frames.min_order - 2;
  if (meta_order <= 2) meta_order = 2;
  if (meta_order < frames.min_order) meta_order = frames.min_order;
  frames.metadata = take_block(&frames, meta_order);
  if (!frames.metadata) FRAMES_PANIC(0, "not enough memory for frame metadata\n");

  TRACE("setting initial metadata\n");

  memset(frames.metadata, 0, 1 << meta_order);
  /* synchronise metadata with initial blocks */
  mark_blocks(&frames, start, frames.max_order, mem_info, data);

  TRACE("  done\n");

  /* execute metadata changes for the metadata block allocation */
  {
    TRACE("replaying metadata for %p order %u\n",
          frames.metadata, meta_order);
    int index = frames_block_index(&frames, frames.metadata, meta_order);
    while (index > 0) {
      TRACE("setting bit for index %d\n", index);
      SET_BIT(frames.metadata, index);
      index = (index >> 1) - 1;
    }
  }

  /* allocate frames_t structure itself */
  TRACE("allocating frames_t (size 0x%lx)\n", sizeof(frames_t));
  frames_t *ret = frames_alloc(&frames, sizeof(frames_t));
  *ret = frames;

  return ret;
}

size_t frames_available_memory(frames_t *frames)
{
  size_t total = 0;
  for (unsigned int k = frames->min_order; k <= frames->max_order; k++) {
    size_t size = 1 << k;
    block_t *block = *block_head(frames, k);
    while (block) {
      total += size;
      block = block->next;
    }
  }
  return total;
}

void *frames_alloc(frames_t *frames, size_t sz)
{
  void *ret = take_block(frames, ORDER_OF(sz));
  TRACE("allocated 0x%lx size 0x%lx (order %lu)\n",
         ret - frames->start, sz, ORDER_OF(sz));
  frames_dump_diagnostics(frames);
  return ret;
}

void frames_dump_diagnostics(frames_t *frames)
{
  TRACE("----\n");
  for (unsigned int k = frames->min_order; k <= frames->max_order; k++) {
    block_t *block = *block_head(frames, k);
    int nonempty = block != 0;
    if (nonempty) TRACE("order %d: ", k);
    while (block) {
      TRACE("0x%lx ", (void *)block - frames->start);
      block = block->next;
    }
    if (nonempty) TRACE("\n");
  }
  TRACE("----\n");
}

void frames_free_order(frames_t *frames, void *p, unsigned int order)
{
  block_t *block = p;

  /* find buddy */
  block_t *buddy = 0;
  if (order < frames->max_order) {
    int index = frames_block_index(frames, block, order);
    UNSET_BIT(frames->metadata, index);
    TRACE("checking buddy for 0x%lx order %d index 0x%x\n",
           p - frames->start, order, index);
    if (!GET_BIT(frames->metadata, index ^ 1)) {
      buddy = frames_index_block(frames, index ^ 1, order);
    }
  }

  /* merge */
  if (buddy) {
    TRACE("  buddy found: 0x%lx\n", (void *)buddy - frames->start);
    list_remove(block_head(frames, order), buddy);
    if (buddy < block) block = buddy;
    frames_free_order(frames, block, order + 1);
    return;
  }
  else {
    list_add(block_head(frames, order), block);
  }

  TRACE("freed 0x%lx of order %d\n", p - frames->start, order);
  frames_dump_diagnostics(frames);
}

unsigned int frames_find_order(frames_t *frames, void *p)
{
  unsigned int order = __builtin_ctzl(p - frames->start);
  TRACE("maximum order for 0x%lx: %d\n", p - frames->start, order);

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

void frames_free(frames_t *frames, void *p)
{
  frames_free_order(frames, p, frames_find_order(frames, p));
}

int default_mem_info(void *start, size_t size, void *data)
{
  return MEM_INFO_USABLE;
}

#include "frames.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define FRAMES_PANIC(ret, ...) do { \
  fprintf(stderr, "[frames]"); \
  fprintf(stderr, __VA_ARGS__); \
  return ret; } while(0)

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
Blocks are numbered as follows:

_______________
_______________
___0___|___1___
_2_|_3_|_4_|_5_
6|7|8|9|a|b|c|d
      ...

Note that the unique block of maximum order is not numbered. This is for two reasons:
 - it does not have a buddy
 - taking the buddy of a block just corresponds to flipping the lowest bit of its index. */
struct frames_t {
  void *start;
  struct block_t *free[MAX_ORDER];
  unsigned int min_order, max_order;

  /* This is bit vector divided into two sections:
     (let N = 1 << (max_order - min_order - 1), i.e. half the number of blocks, rounded up)

     - from 0 to N - 1:
         free(block(2i)) xor free(block(2i+1))
     - from N to 2N:
         split(block(i - N))

     Note how we only use 2N bits instead of 4N. I learned this trick from
     <https://bitsquid.blogspot.com/2015/08/allocation-adventures-3-buddy-allocator.html>

     At the beginning, metadata can be null. This is important when we
     are allocating space for the metadata table itself. */
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
  int info = mem_info(start, 1 << order, data);

  /* if the block is usable, just add it to the list */
  if (info == MEM_INFO_USABLE) {
    block_t *block = (block_t *)start;
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
  int info = mem_info(start, 1 << order, data);
  if (info == MEM_INFO_USABLE) return;

  unsigned int index = frames_block_index(frames, start, order);
  SET_BIT(frames->metadata, index);

  if (info == MEM_INFO_PARTIALLY_USABLE) {
    mark_blocks(frames, start, order - 1, mem_info, data);
    mark_blocks(frames, start + (1 << (order - 1)), order - 1, mem_info, data);
  }
}

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
    printf("set bit order %u index 0x%x\n", order,
           frames_block_index(frames, block, order));
    SET_BIT(frames->metadata, frames_block_index(frames, block, order));
  }
  return block;
}

frames_t *frames_new(void *start, size_t size,
                     unsigned int min_order,
                     int (*mem_info)(void *start, size_t size, void *data),
                     void *data)
{
  if (min_order < ORDER_OF(sizeof(block_t))) {
    FRAMES_PANIC(0, "min_order must be at least %lu\n", ORDER_OF(sizeof(block_t)));
  }

  frames_t frames;
  frames.start = start;
  frames.min_order = min_order;
  frames.max_order = ORDER_OF(size);
  if (frames.max_order < frames.min_order) {
    FRAMES_PANIC(0, "min_order too large\n");
  }
  for (unsigned int k = frames.min_order; k <= frames.max_order; k++) {
    *block_head(&frames, k) = 0;
  }

  frames_dump_diagnostics(&frames);

  /* add all blocks */
  add_blocks(frames.max_order, start, &frames, mem_info, data);

  frames_dump_diagnostics(&frames);

  /* set metadata */
  frames.metadata = 0;
  int meta_order = frames.max_order - frames.min_order - 2;
  if (meta_order <= 2) meta_order = 2;
  frames.metadata = take_block(&frames, meta_order);
  if (!frames.metadata) FRAMES_PANIC(0, "not enough memory for frame metadata");

  frames_dump_diagnostics(&frames);

  memset(frames.metadata, 0, 1 << meta_order);
  mark_blocks(&frames, start, frames.max_order, mem_info, data);

  /* allocate frames_t structure itself */
  printf("allocating frames_t (size 0x%lx)\n", sizeof(frames_t));
  frames_t *ret = frames_alloc(&frames, sizeof(frames_t));
  *ret = frames;

  return ret;
}

size_t frames_available_memory(frames_t *frames)
{
  size_t total = 0;
  for (unsigned int k = frames->min_order; k <= frames->max_order; k++) {
    size_t size = 1 << k;
    block_t *block = frames->free[k - frames->min_order];
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
  printf("allocated 0x%lx size 0x%lx (order %lu)\n",
         ret - frames->start, sz, ORDER_OF(sz));
  frames_dump_diagnostics(frames);
  return ret;
}

void frames_dump_diagnostics(frames_t *frames)
{
  printf("----\n");
  for (unsigned int k = frames->min_order; k <= frames->max_order; k++) {
    block_t *block = *block_head(frames, k);
    int nonempty = block != 0;
    if (nonempty) printf("order %d: ", k);
    while (block) {
      printf("0x%lx ", (void *)block - frames->start);
      block = block->next;
    }
    if (nonempty) printf("\n");
  }
  printf("----\n");
}

void frames_free_order(frames_t *frames, void *p, unsigned int order)
{
  block_t *block = p;

  /* find buddy */
  block_t *buddy = 0;
  if (order < frames->max_order) {
    int index = frames_block_index(frames, block, order);
    UNSET_BIT(frames->metadata, index);
    printf("checking buddy for 0x%lx order %d index 0x%x\n",
           p - frames->start, order, index);
    if (!GET_BIT(frames->metadata, index ^ 1)) {
      buddy = frames_index_block(frames, index ^ 1, order);
    }
  }

  /* merge */
  if (buddy) {
    printf("  buddy found: 0x%lx\n", (void *)buddy - frames->start);
    list_remove(block_head(frames, order), buddy);
    if (buddy < block) block = buddy;
    frames_free_order(frames, block, order + 1);
    return;
  }
  else {
    list_add(block_head(frames, order), block);
  }

  printf("freed 0x%lx of order %d\n", p - frames->start, order);
  frames_dump_diagnostics(frames);
}

unsigned int frames_find_order(frames_t *frames, void *p)
{
  unsigned int order = __builtin_ctzl(p - frames->start);
  printf("maximum order for 0x%lx: %d\n", p - frames->start, order);

  for (unsigned int k = order - 1; k >= frames->min_order; k--) {
    unsigned int i = frames_block_index(frames, p, k);
    printf("  order %u index 0x%x: %s\n", k, i, GET_BIT(frames->metadata, i) ? "yes" : "no");
  }
  printf("  ---\n");

  unsigned int index = frames_block_index(frames, p, order);
  for (; order > frames->min_order; order--) {
    index = (index << 1) + 2; /* first child */
    printf("  bit order %u index 0x%x: %s\n",
           order - 1, index,
           GET_BIT(frames->metadata, index) ? "yes" : "no");
    if (!GET_BIT(frames->metadata, index)) break;
  }
  printf("  order %d\n", order);
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

#include <stdio.h>
#include <stdlib.h>

#include "../kernel/frames.h"

#include "test_assert.h"

frames_t *frames_new(uint64_t start, uint64_t end, unsigned int min_order,
                     int (*mem_info)(uint64_t start, uint64_t size, void *data),
                     void *data)
{
  frames_t *frames = malloc(sizeof(frames_t));
  frames_init(frames, 0, start, end, min_order, mem_info, data);
  return frames;
}

static int test_alloc_free(void *mem, size_t sz)
{
  frames_t *frames = frames_new((uint64_t) mem,
                                (uint64_t) mem + sz,
                                5, default_mem_info, 0);
  T_ASSERT(frames);
  size_t total = frames_available_memory(frames);

  void *x = (void *) frames_alloc(frames, 128);
  T_ASSERT(x);
  T_ASSERT_EQ(frames_available_memory(frames), total - 128);

  frames_free(frames, (uint64_t) x);
  T_ASSERT_EQ(frames_available_memory(frames), total);

  x = (void *) frames_alloc(frames, 200);
  T_ASSERT(x);
  T_ASSERT_EQ(frames_available_memory(frames), total - 256);
  void *y = (void *) frames_alloc(frames, 33);
  T_ASSERT(y);
  T_ASSERT_EQ(frames_available_memory(frames), total - 256 - 64);
  frames_free(frames, (uint64_t) x);
  T_ASSERT_EQ(frames_available_memory(frames), total - 64);
  frames_free(frames, (uint64_t) y);
  T_ASSERT_EQ(frames_available_memory(frames), total);

  free(frames);

  return 0;
}

typedef struct {
  void *start;
  size_t size;
} chunk_t;

static int chunk_mem_info(uint64_t start, uint64_t size, void *data) {
  chunk_t *chunk = data;
  if (start < (uint64_t) chunk->start) {
    if (start + size > (uint64_t) chunk->start)
      return MEM_INFO_PARTIALLY_USABLE;
    else
      return MEM_INFO_RESERVED;
  }
  else if (start >= (uint64_t) chunk->start + chunk->size) {
    return MEM_INFO_RESERVED;
  }
  else if (start + size > (uint64_t) chunk->start + chunk->size) {
    return MEM_INFO_PARTIALLY_USABLE;
  }
  else {
    return MEM_INFO_USABLE;
  }
}

static int test_chunk_alloc(void *mem, size_t sz0) {
  size_t sz = 12000;
  T_ASSERT(sz <= sz0);

  chunk_t chunk;
  chunk.start = mem;
  chunk.size = sz;

  frames_t *frames = frames_new((uint64_t) mem,
                                (uint64_t) mem + sz,
                                5, chunk_mem_info, &chunk);
  T_ASSERT(frames);
  size_t total = frames_available_memory(frames);
  T_ASSERT(total < sz);

  void *x = (void *) frames_alloc(frames, 6500);
  T_ASSERT(x);
  T_ASSERT_EQ(frames_available_memory(frames), total - 0x2000);

  void *y = (void *) frames_alloc(frames, 6500);
  T_ASSERT(!y);

  free(frames);

  return 0;
}

static int test_order_of()
{
  T_ASSERT(ORDER_OF(0xf) == 4);
  T_ASSERT(ORDER_OF(0x10) == 4);
  T_ASSERT(ORDER_OF(0x1c) == 5);
  T_ASSERT(ORDER_OF(0x73a8bb2) == 27);
  T_ASSERT(ORDER_OF(0x100000) == 20);
  return 0;
}

int buddy_test(void)
{
  size_t sz = 0x100000;
  void *mem = malloc(sz);

  int err = 0;
  err = test_alloc_free(mem, sz) || err;
  err = test_chunk_alloc(mem, sz) || err;
  err = test_order_of() || err;

  free(mem);
  return err;
}

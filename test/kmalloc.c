#include <stdio.h>
#include <stdlib.h>

#include "../kernel/heap.h"
#include "../kernel/frames.h"

#include "test_assert.h"

#define POOL_SIZE (16 * 1024)
static frames_t frames;
static uint8_t pool[POOL_SIZE];

static int mem_info(uint64_t start, uint64_t size, void *data)
{
  return MEM_INFO_USABLE;
}

static int disjoint(void *p1, size_t l1, void *p2, size_t l2)
{
  return p1 + l1 < p2 || p1 >= p2 + l2;
}

static int test_alloc_disjoint(void)
{
  frames_init(&frames, 0,
              (size_t) pool,
              (size_t) (pool + POOL_SIZE),
              8, mem_info, 0);
  heap_t *heap = heap_new_with_growth(&frames, 1);
  T_ASSERT(heap);

  static size_t sizes[] = { 147, 55, 23, 31, 9 };
  static const int num_allocs = sizeof(sizes) / sizeof(size_t);
  void *p[num_allocs];

  for (int i = 0; i < num_allocs; i++) {
    p[i] = heap_malloc(heap, sizes[i]);
  }

  for (int i = 0; i < num_allocs; i++) {
    for (int j = i + 1; j < num_allocs; j++) {
      T_ASSERT_MSG(disjoint(p[i], sizes[i], p[j], sizes[j]),
                   "%d and %d not disjoint", i, j);
    }
  }

  return 0;
}

static int test_alloc_free_disjoint(void)
{
  frames_init(&frames, 0,
              (size_t) pool,
              (size_t) (pool + POOL_SIZE),
              8, mem_info, 0);
  heap_t *heap = heap_new_with_growth(&frames, 1);
  T_ASSERT(heap);

  static size_t sizes[] = { 147, 55, 23, 31, 9, 21, 5 };
  static const int num_allocs = sizeof(sizes) / sizeof(size_t);
  void *p[num_allocs];

  void *q1 = heap_malloc(heap, 71);
  p[0] = heap_malloc(heap, sizes[0]);
  p[1] = heap_malloc(heap, sizes[1]);
  void *q2 = heap_malloc(heap, 3);
  p[2] = heap_malloc(heap, sizes[2]);
  p[3] = heap_malloc(heap, sizes[3]);
  void *q3 = heap_malloc(heap, 39);
  p[4] = heap_malloc(heap, sizes[4]);
  heap_free(heap, q2);
  p[5] = heap_malloc(heap, sizes[5]);
  void *q4 = heap_malloc(heap, 12);
  heap_free(heap, q3);
  heap_free(heap, q1);
  p[6] = heap_malloc(heap, sizes[6]);
  heap_free(heap, q4);

  for (int i = 0; i < num_allocs; i++) {
    for (int j = i + 1; j < num_allocs; j++) {
      T_ASSERT_MSG(disjoint(p[i], sizes[i], p[j], sizes[j]),
                   "%d and %d not disjoint", i, j);
    }
  }

  return 0;
}

int kmalloc_test(void)
{
  int err = test_alloc_disjoint();
  err = test_alloc_free_disjoint() || err;

  return err;
}

#include <stdio.h>
#include <stdlib.h>

#include "frames.h"

#define ASSERT_MSG(x, msg, ...) do {\
  if (!(x)) { \
    fprintf(stderr, "ASSERT (%s:%d): " msg "\n", \
            __FILE__, __LINE__ __VA_OPT__(,) __VA_ARGS__);  \
    return 1; \
  } } while (0)
#define ASSERT(x) ASSERT_MSG(x, #x)
#define ASSERT_EQ(x, y) ASSERT_MSG(((x) == (y)), \
  "equality test failed:\n  " #x " = 0x%lx\n  " #y " = 0x%lx", x, y)

int test_alloc_free(void *mem, size_t sz)
{
  frames_t *frames = frames_new(mem, 5, 20, default_mem_info, 0);
  ASSERT(frames);
  size_t total = frames_available_memory(frames);
  printf("total memory: 0x%lx\n", total);

  void *x = frames_alloc(frames, 128);
  ASSERT(x);
  ASSERT_EQ(frames_available_memory(frames), total - 128);

  frames_free(frames, x);
  ASSERT_EQ(frames_available_memory(frames), total);

  x = frames_alloc(frames, 200);
  ASSERT(x);
  ASSERT_EQ(frames_available_memory(frames), total - 256);
  void *y = frames_alloc(frames, 33);
  ASSERT(y);
  ASSERT_EQ(frames_available_memory(frames), total - 256 - 64);
  frames_free(frames, x);
  ASSERT_EQ(frames_available_memory(frames), total - 64);
  frames_free(frames, y);
  ASSERT_EQ(frames_available_memory(frames), total);

  return 0;
}

int test_order_of()
{
  ASSERT(ORDER_OF(0xf) == 4);
  ASSERT(ORDER_OF(0x10) == 4);
  ASSERT(ORDER_OF(0x1c) == 5);
  ASSERT(ORDER_OF(0x73a8bb2) == 27);
  ASSERT(ORDER_OF(0x100000) == 20);
  return 0;
}

int main(int argc, char **argv)
{
  size_t sz = 0x100000;
  void *mem = malloc(sz);

  int err = 0;
  err = test_alloc_free(mem, sz) || err;
  err = test_order_of() || err;
  return err;
}

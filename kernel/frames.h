#ifndef FRAMES_H
#define FRAMES_H

#include <stdint.h>
#include <stddef.h>

#define MAX_ORDER 64
#define ORDER_OF(n) ((unsigned int)(sizeof(size_t) * 8 - __builtin_clzl((n) - 1)))

#define KERNEL_MEMORY_END (126 * 1024 * 1024)
#define USER_MEMORY_START (128 * 1024 * 1024)

typedef struct frames {
  uint64_t start;
  uint64_t end;
  uint64_t free[MAX_ORDER];
  unsigned int min_order, max_order;

  uint32_t *metadata;
} frames_t;

typedef struct {
  void *start;
  size_t size;
} mem_block_t;

enum {
  MEM_INFO_RESERVED = 0,
  MEM_INFO_PARTIALLY_USABLE,
  MEM_INFO_USABLE,
};

int default_mem_info(uint64_t start, uint64_t size, void *data);

int frames_init(frames_t *frames,
                uint64_t start, uint64_t end,
                unsigned int min_order,
                int (*mem_info)(uint64_t start, uint64_t size, void *data),
                void *data);
size_t frames_available_memory(frames_t *frames);
uint64_t frames_alloc(frames_t *frames, size_t sz);
void frames_free(frames_t *frames, uint64_t p);
void frames_dump_diagnostics(frames_t *frames);

#endif /* FRAMES_H */

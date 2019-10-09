#ifndef FRAMES_H
#define FRAMES_H

#include <stdint.h>
#include <stddef.h>

#define MAX_ORDER 64
#define ORDER_OF(n) ((unsigned int)(sizeof(size_t) * 8 - __builtin_clzl((n) - 1)))
#define MAX_KERNEL_MEMORY_SIZE (126 * 1024 * 1024)

struct frames;
typedef struct frames frames_t;

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

frames_t *frames_new(uint64_t start, uint64_t end,
                     unsigned int min_order,
                     int (*mem_info)(uint64_t start, uint64_t size, void *data),
                     void *data);
size_t frames_available_memory(frames_t *frames);
uint64_t frames_alloc(frames_t *frames, size_t sz);
void frames_free(frames_t *frames, uint64_t p);
void frames_dump_diagnostics(frames_t *frames);

#endif /* FRAMES_H */

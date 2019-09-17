#ifndef FRAMES_H
#define FRAMES_H

#include <stdint.h>
#include <stddef.h>

#define MAX_ORDER 32
#define ORDER_OF(n) (sizeof(unsigned long) * 8 - __builtin_clzl((n) - 1))

struct frames_t;
typedef struct frames_t frames_t;

typedef struct {
  void *start;
  size_t size;
} mem_block_t;

enum {
  MEM_INFO_RESERVED = 0,
  MEM_INFO_PARTIALLY_USABLE,
  MEM_INFO_USABLE,
};

int default_mem_info(void *start, size_t size, void *data);

frames_t *frames_new(void *start, size_t size,
                     unsigned int min_order,
                     int (*mem_info)(void *start, size_t size, void *data),
                     void *data);
size_t frames_available_memory(frames_t *frames);
void *frames_alloc(frames_t *frames, size_t sz);
void frames_free(frames_t *frames, void *p);
void frames_dump_diagnostics(frames_t *frames);

#endif /* FRAMES_H */

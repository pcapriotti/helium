#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

#define ALIGNED(x, bits) (((uint32_t) x) & (~0UL << (bits)))
#define PAGE_BITS 12

extern uint8_t _stage1_end[];
extern uint8_t _kernel_start[];
extern uint8_t _kernel_end[];

typedef struct {
  uint64_t base;
  int type;
} chunk_t;

struct frames;
extern struct frames kernel_frames;

void *falloc(size_t sz);
void ffree(void *p);

chunk_t *memory_get_chunks(int *count, uint32_t **heap);
void memory_reserve_chunk(chunk_t *chunks, int *num_chunks,
                          uint64_t start, uint64_t end);
int memory_init(uint32_t *heap);

#endif /* MEMORY_H */

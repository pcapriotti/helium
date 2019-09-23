#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_BITS 12

extern uint8_t _stage1_end[];
extern uint8_t _kernel_start[];
extern uint8_t _kernel_end[];
extern uint8_t _bss_start[];
extern uint8_t _bss_end[];

typedef struct {
  uint64_t base;
  int type;
} chunk_t;

struct frames;
extern struct frames *memory_frames;

void *falloc(size_t sz);
void ffree(void *p);

chunk_t *memory_get_chunks(int *count, void *heap);
void memory_reserve_chunk(chunk_t *chunks, int *num_chunks,
                          uint64_t start, uint64_t end);
int memory_init(void *heap);

#endif /* MEMORY_H */

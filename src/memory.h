#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

extern uint8_t _stage1_end[];
extern uint8_t _kernel_start[];
extern uint8_t _kernel_end[];
extern uint8_t _kernel_low_start[];
extern uint8_t _kernel_low_end[];

typedef struct {
  uint64_t base;
  int type;
} chunk_t;

chunk_t *memory_get_chunks(int *count, void *heap);
void memory_reserve_chunk(chunk_t *chunks, int *num_chunks,
                          uint64_t start, uint64_t end);

#endif /* MEMORY_H */

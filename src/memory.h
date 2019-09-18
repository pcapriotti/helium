#ifndef MEMORY_H
#define MEMORY_H

#include "stdint.h"

extern uint8_t _stage1_end[];
extern uint8_t _kernel_start[];
extern uint8_t _kernel_end[];
extern uint8_t _kernel_low_start[];
extern uint8_t _kernel_low_end[];

typedef struct {
  uint64_t base;
  uint64_t size;
  uint64_t type;
} __attribute__((packed)) memory_map_entry_t;

memory_map_entry_t *get_memory_map(size_t *count);

#endif /* MEMORY_H */

#ifndef PAGING_H
#define PAGING_H

#include "memory.h"

typedef struct page {
  uint8_t bytes[1 << PAGE_BITS];
} __attribute__((packed, aligned(1 << PAGE_BITS))) page_t;

typedef uint32_t pt_entry_t;
typedef pt_entry_t page_table_t[1 << (PAGE_BITS - 2)];

enum {
  PT_ENTRY_PRESENT = 1 << 0,
  PT_ENTRY_RW = 1 << 1,
  PT_ENTRY_USER = 1 << 2,
  PT_ENTRY_PWT = 1 << 3,
  PT_ENTRY_PCD = 1 << 4,
  PT_ENTRY_ACCESSED = 1 << 5,
  PT_ENTRY_DIRTY = 1 << 6,
  PT_ENTRY_SIZE = 1 << 7,
  PT_ENTRY_GLOBAL = 1 << 8,
};

void paging_init(void);
void paging_idmap(void *address);

#endif /* PAGING_H */

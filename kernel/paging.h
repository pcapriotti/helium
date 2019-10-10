#ifndef PAGING_H
#define PAGING_H

#include "memory.h"

#include <string.h>

#define PAGE(x) ((page_t *) ALIGNED(x, PAGE_BITS))

#define PAGING_DEBUG 1

typedef struct page {
  uint8_t bytes[1 << PAGE_BITS];
} __attribute__((packed, aligned(1 << PAGE_BITS))) page_t;

static inline void page_zero(page_t *page)
{
  memset(page, 0, sizeof(page_t));
}

/* kernel virtual memory is as follows:

  0 - 124 MB: identity mapping
  124 MB - 128 MB: temporary mappings
  128 MB - 256 MB: permanent mappings
*/
#define KERNEL_VM_ID_START 0
#define KERNEL_VM_ID_END ((void *)(124 * 1024 * 1024))
#define KERNEL_VM_TEMP_START KERNEL_VM_ID_END
#define KERNEL_VM_TEMP_END ((void *)(128 * 1024 * 1024))
#define KERNEL_VM_PERM_START KERNEL_VM_TEMP_END
#define KERNEL_VM_PERM_END ((void *)(256 * 1024 * 1024))

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

enum {
  PAGING_DISABLED,
  PAGING_LEGACY,
  PAGING_PAE,
};

typedef struct pg_ops {
  void *(*map_temp)(void *data, uint64_t p);
  void (*unmap_temp)(void *data, void *p);
  void *(*map_perm)(void *data, uint64_t p);
  uint64_t (*max_memory)(void *data);
} pg_ops_t;

extern int paging_type;

void *paging_perm_map_pages(uint64_t p, size_t size);

void *paging_temp_map_page(uint64_t p);
void paging_temp_unmap_page(void * p);

int paging_init(uint64_t memory);

uint64_t paging_maximum_memory();

#endif /* PAGING_H */

#ifndef PAGING_H
#define PAGING_H

#include "memory.h"

typedef struct page {
  uint8_t bytes[1 << PAGE_BITS];
} __attribute__((packed, aligned(1 << PAGE_BITS))) page_t;

typedef uint32_t pt_entry_t;
typedef pt_entry_t page_table_t[1 << (PAGE_BITS - 2)];

/* kernel virtual memory is as follows:

  0 - 126 MB: identity mapping
  126 MB - 128 MB: temporary mappings
  128 MB - 256 MB: permanent mappings
*/
#define KERNEL_VM_ID_START 0
#define KERNEL_VM_ID_END ((void *)(126 * 1024 * 1024))
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
  PAGING_REGULAR,
  PAGING_PAE,
};

extern int paging_state;

int paging_init(void);
void paging_idmap(void *address);

void *paging_perm_map_page(uint64_t p);
void *paging_perm_map_pages(uint64_t p, size_t size);

#endif /* PAGING_H */

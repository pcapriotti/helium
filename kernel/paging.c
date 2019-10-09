#include "core/debug.h"
#include "core/interrupts.h"
#include "paging.h"

#include <string.h>

#define PAGING_DEBUG 0
#if PAGING_DEBUG
#define TRACE(...) kprintf(__VA_ARGS__)
#else
#define TRACE(...) do {} while(0)
#endif

#define LARGE_PAGE_BITS (PAGE_BITS + PAGE_BITS - 2)
#define LARGE_PAGE(x) ((page_t *) ALIGNED(x, LARGE_PAGE_BITS))
#define PAGE(x) ((page_t *) ALIGNED(x, PAGE_BITS))

#define DIR_INDEX(x) (((uint32_t) x) >> LARGE_PAGE_BITS)

int paging_state = PAGING_DISABLED;

static inline void page_zero(page_t *page)
{
  memset(page, 0, sizeof(page_t));
}

static inline pt_entry_t entry(page_t *page, uint16_t flags)
{
  return (uint32_t) page | flags;
}

void paging_idmap_large(pt_entry_t *table, void *address)
{
  table[DIR_INDEX(address)] =
    entry(LARGE_PAGE(address),
          PT_ENTRY_PRESENT |
          PT_ENTRY_RW |
          PT_ENTRY_SIZE);
}

/* identity map a large page containing a given address */
void paging_idmap(void *address)
{
  pt_entry_t *table = (pt_entry_t *) CR_GET(3);
  paging_idmap_large(table, address);
}

int paging_init(void)
{
  // TODO: use a page allocator
  page_t *directory = falloc(sizeof(page_t));
  page_zero(directory);
  pt_entry_t *dir_table = (pt_entry_t *) directory;

  /* identity map the first 4 MB with a single large page */
  paging_idmap_large(dir_table, 0);
  /* identity map a large paging containing the page directory */
  paging_idmap_large(dir_table, dir_table);

  /* install page directory */
  CR_SET(3, directory);

  /* enable large pages */
  if (!cpuid_check_features(CPUID_FEAT_PSE)) {
    TRACE("Large pages not supported\n");
    return -1;
  }
  CR_SET(4, CR_GET(4) | CR4_PSE);

  paging_enable();
  paging_state = PAGING_REGULAR;

  return 0;
}

#include "core/debug.h"
#include "core/interrupts.h"
#include "frames.h"
#include "paging.h"

#include <assert.h>
#include <string.h>
#include <inttypes.h>

#define PAGING_DEBUG 1
#if PAGING_DEBUG
#define TRACE(...) serial_printf(__VA_ARGS__)
#else
#define TRACE(...) do {} while(0)
#endif

#define LARGE_PAGE_BITS (PAGE_BITS + PAGE_BITS - 2)
#define LARGE_PAGE(x) ((page_t *) ALIGNED(x, LARGE_PAGE_BITS))
#define PAGE(x) ((page_t *) ALIGNED(x, PAGE_BITS))

#define DIR_INDEX(x) (((uint32_t) x) >> LARGE_PAGE_BITS)
#define TABLE_INDEX(x) ((((uint32_t) x) >> PAGE_BITS) & ((1 << (PAGE_BITS - 2)) - 1))

int paging_state = PAGING_DISABLED;
page_t *paging_perm = KERNEL_VM_PERM_START;
page_t *paging_temp = KERNEL_VM_TEMP_START;
pt_entry_t *dir_table = 0;
pt_entry_t *tmp_table = 0;

static inline void page_zero(page_t *page)
{
  memset(page, 0, sizeof(page_t));
}

static inline pt_entry_t mk_entry(page_t *page, uint16_t flags)
{
  return (uint32_t) page | flags;
}

void paging_idmap_large(pt_entry_t *table, void *address)
{
  table[DIR_INDEX(address)] =
    mk_entry(LARGE_PAGE(address),
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
  dir_table = (pt_entry_t *) directory;

  /* identity map kernel memory */
  for (void *p = KERNEL_VM_ID_START; p < KERNEL_VM_ID_END; p += (1 << LARGE_PAGE_BITS)) {
    paging_idmap_large(dir_table, p);
  }

  /* set up temporary mapping table */
  {
    page_t *tmp_page = falloc(sizeof(page_t));
    page_zero(tmp_page);
    dir_table[DIR_INDEX(KERNEL_VM_TEMP_START)] = mk_entry
      (tmp_page, PT_ENTRY_PRESENT | PT_ENTRY_RW);
    tmp_table = (pt_entry_t *) tmp_page;
  }

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

/* map a physical page into the temporary VMA space */
void *paging_temp_map_page(uint64_t p)
{
  TRACE("temp map: %#" PRIx64 "\n", p);

  assert(paging_state == PAGING_REGULAR);
  assert(dir_table);
  assert(p < (1ULL << 32));

  page_t *tmp = paging_temp;
  uint32_t *entry = 0;
  do {
    entry = &tmp_table[TABLE_INDEX(tmp)];
    if (!(*entry & PT_ENTRY_PRESENT)) break;
    tmp++;
    if ((void *)tmp >= KERNEL_VM_TEMP_END)
      tmp = KERNEL_VM_TEMP_START;
  } while (tmp != paging_temp);

  if (*entry & PT_ENTRY_PRESENT) {
    kprintf("ERROR: Out of temporary mappings\n");
    panic();
    return 0;
  }

  *entry = mk_entry(PAGE(p), PT_ENTRY_PRESENT | PT_ENTRY_RW);

  paging_temp = tmp + 1;
  if ((void *) paging_temp >= KERNEL_VM_TEMP_END)
    paging_temp = KERNEL_VM_TEMP_START;

  return tmp;
}

void paging_temp_unmap_page(void *p)
{
  assert(p >= KERNEL_VM_TEMP_START && p < KERNEL_VM_TEMP_END);
  assert(((size_t) p & ((1 << PAGE_BITS) - 1)) == 0);

  tmp_table[TABLE_INDEX(p)] = 0;

  paging_temp = p;
}

/* map a single physical page into the permanent VMA space */
void *paging_perm_map_page(uint64_t p)
{
  assert(paging_state == PAGING_REGULAR);
  assert(dir_table);
  assert(p < (1ULL << 32));

  TRACE("perm map: %#" PRIx64 "\n", p);

  uint32_t *entry = &dir_table[DIR_INDEX(paging_perm)];
  page_t *tpage = 0;
  if (*entry & PT_ENTRY_PRESENT) {
    tpage = PAGE(*entry);
  }
  else {
    /* allocate new page table */
    tpage = falloc(sizeof(page_t));
    page_zero(tpage);
    *entry = mk_entry(tpage, PT_ENTRY_PRESENT | PT_ENTRY_RW);
  }

  pt_entry_t *table = (pt_entry_t *)tpage;
  table[TABLE_INDEX(paging_perm)] =
    mk_entry(PAGE(p), PT_ENTRY_PRESENT | PT_ENTRY_RW);

  return paging_perm++;
}

void *paging_perm_map_pages(uint64_t p, size_t size)
{
  uint64_t p1 = p + size;
  void *ret = 0;

  while (p < p1) {
    void *vma = paging_perm_map_page(p);
    if (!ret) ret = vma;
    p += (1 << PAGE_BITS);
  }

  return ret;
}

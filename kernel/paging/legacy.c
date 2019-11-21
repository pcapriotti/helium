#include "core/debug.h"
#include "core/interrupts.h"
#include "core/serial.h"
#include "core/util.h"
#include "frames.h"
#include "paging/paging.h"
#include "paging/legacy.h"

#include <assert.h>
#include <string.h>
#include <inttypes.h>

#if PAGING_DEBUG
#define TRACE(...) serial_printf(__VA_ARGS__)
#else
#define TRACE(...) do {} while(0)
#endif

#define DIR_INDEX(x) (((uint32_t) x) >> LARGE_PAGE_BITS)
#define TABLE_INDEX(x) ((((uint32_t) x) >> PAGE_BITS) & ((1 << (PAGE_BITS - 2)) - 1))

#define LARGE_PAGE_BITS (PAGE_BITS + PAGE_BITS - 2)
#define LARGE_PAGE(x) ((page_t *) ALIGN_BITS(x, LARGE_PAGE_BITS))

static inline pg_legacy_entry_t mk_entry(page_t *page, uint16_t flags)
{
  return (uint32_t) page | flags;
}

static void paging_idmap_large(pg_legacy_entry_t *table, void *address)
{
  table[DIR_INDEX(address)] =
    mk_entry(LARGE_PAGE((size_t) address),
             PT_ENTRY_PRESENT |
             PT_ENTRY_RW |
             PT_ENTRY_SIZE);
}

int paging_legacy_init(paging_legacy_t *pg)
{
  pg->perm = KERNEL_VM_PERM_START;
  pg->temp = KERNEL_VM_TEMP_START;

  // TODO: use a page allocator
  page_t *directory = falloc(sizeof(page_t));
  page_zero(directory);
  pg->dir_table = (pg_legacy_entry_t *) directory;

  /* identity map kernel memory */
  for (void *p = KERNEL_VM_ID_START; p < KERNEL_VM_ID_END; p += (1 << LARGE_PAGE_BITS)) {
    paging_idmap_large(pg->dir_table, p);
  }

  /* set up temporary mapping table */
  {
    page_t *tmp_page = falloc(sizeof(page_t));
    page_zero(tmp_page);
    pg->dir_table[DIR_INDEX(KERNEL_VM_TEMP_START)] = mk_entry
      (tmp_page, PT_ENTRY_PRESENT | PT_ENTRY_RW);
    pg->tmp_table = (pg_legacy_entry_t *) tmp_page;
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

  return 0;
}

/* map a physical page into the temporary VMA space */
static void *paging_legacy_map_temp(void *data, uint64_t p)
{
  paging_legacy_t *pg = data;

  TRACE("temp map: %#" PRIx64 "\n", p);

  assert(pg->dir_table);
  assert(p < (1ULL << 32));

  page_t *tmp = pg->temp;
  uint32_t *entry = 0;
  do {
    entry = &pg->tmp_table[TABLE_INDEX(tmp)];
    if (!(*entry & PT_ENTRY_PRESENT)) break;
    tmp++;
    if ((void *)tmp >= KERNEL_VM_TEMP_END)
      tmp = KERNEL_VM_TEMP_START;
  } while (tmp != pg->temp);

  if (*entry & PT_ENTRY_PRESENT) {
    serial_set_colour(SERIAL_COLOUR_ERR);
    serial_printf("ERROR: Out of temporary mappings\n");
    panic();
    return 0;
  }

  *entry = mk_entry(PAGE((size_t) p), PT_ENTRY_PRESENT | PT_ENTRY_RW);

  pg->temp = tmp + 1;
  if ((void *) pg->temp >= KERNEL_VM_TEMP_END)
    pg->temp = KERNEL_VM_TEMP_START;

  return tmp;
}

static void paging_legacy_unmap_temp(void *data, void *p)
{
  paging_legacy_t *pg = data;

  assert(p >= KERNEL_VM_TEMP_START && p < KERNEL_VM_TEMP_END);
  assert(((size_t) p & ((1 << PAGE_BITS) - 1)) == 0);

  __asm__ volatile("invlpg %0" : : "m"(*(uint8_t *)p));
  pg->tmp_table[TABLE_INDEX(p)] = 0;

  pg->temp = p;
}

/* map a single physical page into the permanent VMA space */
static void *paging_legacy_map_perm(void *data, uint64_t p)
{
  paging_legacy_t *pg = data;

  assert(pg->dir_table);
  assert(p < (1ULL << 32));

  TRACE("perm map: %#" PRIx64 "\n", p);

  uint32_t *entry = &pg->dir_table[DIR_INDEX(pg->perm)];
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

  pg_legacy_entry_t *table = (pg_legacy_entry_t *)tpage;
  table[TABLE_INDEX(pg->perm)] =
    mk_entry(PAGE((size_t) p), PT_ENTRY_PRESENT | PT_ENTRY_RW);

  return pg->perm++;
}

static uint64_t paging_legacy_max_memory(void *data)
{
  return 1ULL << 32;
}

void paging_legacy_init_ops(pg_ops_t *ops)
{
  ops->map_perm = paging_legacy_map_perm;
  ops->map_temp = paging_legacy_map_temp;
  ops->unmap_temp = paging_legacy_unmap_temp;
  ops->max_memory = paging_legacy_max_memory;
}

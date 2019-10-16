#include "core/debug.h"
#include "core/x86.h"
#include "paging/paging.h"
#include "paging/pae.h"

#include <assert.h>

#define ENTRY_BITS (PAGE_BITS - 3)
#define LARGE_PAGE_BITS (PAGE_BITS + ENTRY_BITS)
#define DEF_FLAGS (PT_ENTRY_PRESENT | PT_ENTRY_RW)

/* level 3 table: four entries, highest 2 bits */
#define L3_INDEX(x) ((((uint32_t) x) >> 30) & 0x3)
/* level 2 table: 512 entries, next 9 bits */
#define L2_INDEX(x) ((((uint32_t) x) >> LARGE_PAGE_BITS) & ((1 << ENTRY_BITS) - 1))
/* level 1 table: 512 entries, next 9 bits */
#define L1_INDEX(x) ((((uint32_t) x) >> PAGE_BITS) & ((1 << ENTRY_BITS) - 1))

static inline pg_pae_entry_t mk_entry(uint64_t p, uint16_t flags)
{
  return p | flags;
}

int paging_pae_init(paging_pae_t *pg)
{
  /* allocate level 3 table */
  page_t *l3 = falloc(sizeof(page_t));
  assert(((size_t) l3 & 0xfff) == 0);
  page_zero(l3);
  pg->table3 = (pg_pae_entry_t *)l3;

  /* allocate one level 2 table, this will be enough for the first GB
     of virtual memory */
  page_t *l2 = falloc(sizeof(page_t));
  assert(((size_t) l2 & 0xfff) == 0);
  page_zero(l2);
  pg->table2 = (pg_pae_entry_t *)l2;
  pg->table3[0] = mk_entry((size_t) l2, PT_ENTRY_PRESENT);

  /* identity map kernel memory */
  for (void *p = KERNEL_VM_ID_START; p < KERNEL_VM_ID_END; p += (1 << LARGE_PAGE_BITS)) {
    pg->table2[L2_INDEX(p)] = mk_entry(ALIGNED64((size_t) p, LARGE_PAGE_BITS),
                                       DEF_FLAGS | PT_ENTRY_SIZE);
  }

  /* set up temporary mapping table */
  {
    pg->temp = KERNEL_VM_TEMP_START;
    page_t *tmp_page = falloc(sizeof(page_t));
    page_zero(tmp_page);
    pg->table2[L2_INDEX(pg->temp)] = mk_entry((size_t) tmp_page, DEF_FLAGS);
    pg->tmp_table = (pg_pae_entry_t *)tmp_page;
  }

  /* set up permanent virtual memory area */
  pg->perm = KERNEL_VM_PERM_START;

  /* install level 3 table */
  CR_SET(3, l3);

  /* enable large pages */
  if (!cpuid_check_features(CPUID_FEAT_PSE)) {
    kprintf("ERROR: large pages not supported\n");
    return -1;
  }
  CR_SET(4, CR_GET(4) | CR4_PSE);

  /* enable PAE */
  CR_SET(4, CR_GET(4) | CR4_PAE);

  paging_enable();

  return 0;
}

static void *map_temp(void *data, uint64_t p)
{
  paging_pae_t *pg = data;

  assert(pg->table2);

  page_t *tmp = pg->temp;
  pg_pae_entry_t *entry = 0;
  do {
    entry = &pg->tmp_table[L1_INDEX(tmp)];
    if (!(*entry & PT_ENTRY_PRESENT)) break;
    tmp++;
    if ((void *)tmp >= KERNEL_VM_TEMP_END)
      tmp = KERNEL_VM_TEMP_START;
  } while (tmp != pg->temp);

  if (*entry & PT_ENTRY_PRESENT) {
    serial_printf("ERROR: Out of temporary mappings\n");
    panic();
    return 0;
  }

  *entry = mk_entry(ALIGNED64(p, PAGE_BITS), DEF_FLAGS);

  pg->temp = tmp + 1;
  if ((void *) pg->temp >= KERNEL_VM_TEMP_END)
    pg->temp = KERNEL_VM_TEMP_START;

  return tmp;
}

static void unmap_temp(void *data, void *p)
{
  paging_pae_t *pg = data;

  assert(p >= KERNEL_VM_TEMP_START && p < KERNEL_VM_TEMP_END);
  assert(((size_t) p & ((1 << PAGE_BITS) - 1)) == 0);

  __asm__ volatile("invlpg %0" : : "m"(*(uint8_t *)p));
  pg->tmp_table[L1_INDEX(p)] = 0;

  pg->temp = p;
}

static void *map_perm(void *data, uint64_t p)
{
  paging_pae_t *pg = data;
  assert(pg->table2);

  /* get or create page table */
  pg_pae_entry_t *entry = &pg->table2[L2_INDEX(pg->perm)];
  page_t *tpage = 0;
  if (*entry & PT_ENTRY_PRESENT) {
    tpage = PAGE(*entry);
  }
  else {
    tpage = falloc(sizeof(page_t));
    page_zero(tpage);
    *entry = mk_entry((size_t) tpage, DEF_FLAGS);
  }

  pg_pae_entry_t *table = (pg_pae_entry_t *)tpage;
  table[L1_INDEX(pg->perm)] = mk_entry(ALIGNED64(p, PAGE_BITS), DEF_FLAGS);

  return pg->perm++;
}

static uint64_t max_memory(void *data)
{
  /* TODO: use cpuid */
  return 1ULL << 36;
}

void paging_pae_init_ops(pg_ops_t *ops)
{
  ops->map_temp = map_temp;
  ops->unmap_temp = unmap_temp;
  ops->map_perm = map_perm;
  ops->max_memory = max_memory;
}

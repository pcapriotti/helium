#include "paging.h"
#include "paging/legacy.h"

#include <assert.h>

static pg_ops_t ops;
static paging_legacy_t legacy;
void *ops_data = &legacy;

void *paging_temp_map_page(uint64_t p)
{
  assert(ops.map_temp);
  return ops.map_temp(ops_data, p);
}

void paging_temp_unmap_page(void *p)
{
  assert(ops.unmap_temp);
  ops.unmap_temp(ops_data, p);
}

void *paging_perm_map_page(uint64_t p)
{
  assert(ops.map_perm);
  return ops.map_perm(ops_data, p);
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

int paging_init(void)
{
  if (paging_legacy_init(&legacy) == -1) return -1;
  paging_legacy_init_ops(&ops);
  return 0;
}

uint64_t paging_maximum_memory()
{
  assert(ops.max_memory);
  return ops.max_memory(ops_data);
}

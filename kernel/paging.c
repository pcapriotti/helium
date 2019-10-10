#include "paging.h"
#include "paging/legacy.h"
#include "paging/pae.h"
#include "core/debug.h"
#include "core/x86.h"

#include <assert.h>
#include <inttypes.h>

static pg_ops_t ops;
static paging_legacy_t legacy;
static paging_pae_t pae;
void *ops_data = 0;

void *paging_temp_map_page(uint64_t p)
{
  assert(ops.map_temp);
  void *ret = ops.map_temp(ops_data, p);
#if PAGING_DEBUG
  serial_printf("temp mapping: %#" PRIx64 " => %p\n", p, ret);
#endif
  return ret;
}

void paging_temp_unmap_page(void *p)
{
  assert(ops.unmap_temp);
  ops.unmap_temp(ops_data, p);
#if PAGING_DEBUG
  serial_printf("temp unmap: %p\n", p);
#endif
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

int paging_init(uint64_t memory)
{
  if (memory > (1ULL << 32) && cpuid_check_features(CPUID_FEAT_PAE)) {
#if PAGING_DEBUG
    serial_printf("PAE paging\n");
#endif
    if (paging_pae_init(&pae) == -1) return -1;
    paging_pae_init_ops(&ops);
    ops_data = &pae;
  }
  else {
#if PAGING_DEBUG
    serial_printf("Legacy paging\n");
#endif
    if (paging_legacy_init(&legacy) == -1) return -1;
    paging_legacy_init_ops(&ops);
    ops_data = &legacy;
  }

  return 0;
}

uint64_t paging_maximum_memory()
{
  assert(ops.max_memory);
  return ops.max_memory(ops_data);
}

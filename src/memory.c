#include "debug.h"
#include "memory.h"
#include "stage1.h"
#include "stdint.h"

int mm_compare(const void *x1, const void *x2)
{
  const memory_map_entry_t *e1 = x1;
  const memory_map_entry_t *e2 = x2;

  if (e1->base < e2->base) return -1;
  if (e1->base > e2->base) return 1;
  return 0;
}

enum {
  MM_AVAILABLE = 1,
  MM_RESERVED,
  MM_ACPI_RECLAIMABLE,
  MM_ACPI_NVS,
  MM_UNKNOWN,
};

int mm_type_combine(int t1, int t2) {
  if (t1 == MM_AVAILABLE) return t2;
  if (t2 == MM_AVAILABLE) return t1;
  if (t1 == t2) return t1;
  return MM_RESERVED;
}

void isort(void *base, size_t nmemb, size_t size,
           int (*comp)(const void *, const void *))
{
  for (unsigned int i = 1; i < nmemb; i++) {
    int j = i;
    while (j > 0 && comp(base + (j - 1) * size, base + j * size) > 0) {
      uint8_t *p = base + (j - 1) * size;
      /* swap */
      for (unsigned int k = 0; k < size; k++) {
        int tmp = p[k];
        p[k] = p[k + size];
        p[k + size] = tmp;
      }
      j--;
    }
  }
}

extern int v8086_tracing;

/* Get memory map from BIOS. Since we don't know how much high memory
we have yet, and in particular we have no way to allocate it, we have
to assume that there is enough memory to at least store the memory
map. We put it at the very end of the part of the kernel loaded in low
memory, just after the bss. */
memory_map_entry_t *get_memory_map(size_t *count)
{
  regs16_t regs;

  memory_map_entry_t *result = (memory_map_entry_t *)_kernel_low_end;
  memory_map_entry_t *entry = result;

  regs.ebx = 0;
  do {
    regs.eax = 0xe820;
    regs.edx = 0x534d4150;
    regs.es = 0;
    regs.edi = (uint32_t) entry;
    regs.ecx = sizeof(memory_map_entry_t);
    entry->type = MM_AVAILABLE;
    int flags = bios_int(0x15, &regs);

    if (flags & EFLAGS_CF || regs.eax != 0x534d4150) {
      debug_str("memory map bios call failed\n  eax: ");
      debug_byte(regs.eax >> 24);
      debug_byte(regs.eax >> 16);
      debug_byte(regs.eax >> 8);
      debug_byte(regs.eax);
      debug_str(" eflags: ");
      debug_byte(flags >> 24);
      debug_byte(flags >> 16);
      debug_byte(flags >> 8);
      debug_byte(flags);
      debug_str("\n  num entries so far: ");
      int num = entry - result;
      debug_byte(num >> 8);
      debug_byte(num);
      debug_str("\n");
      return 0;
    }

    /* ignore blocks with unknown type */
    /* if (entry->type >= MM_UNKNOWN || entry->type <= 0) { */
    /*   entry->type = MM_RESERVED; */
    /* } */

    entry++;
  } while (regs.ebx);

  int num_entries = entry - result;

  isort(result, num_entries, sizeof(memory_map_entry_t), &mm_compare);

  /* combine contiguous and overlapping entries */
  memory_map_entry_t *last_entry = result;
  for (int i = 1; i < num_entries; i++) {
    memory_map_entry_t *entry = &result[i];

    /* overlap */
    if (last_entry->base + last_entry->size > entry->base) {
      last_entry->size = entry->size + entry->base - last_entry->base;
      last_entry->type = mm_type_combine(last_entry->type, entry->type);
    }
    /* contiguous */
    else if (last_entry->base + last_entry->size == entry->base &&
             last_entry->type == entry->type) {
      last_entry->size += entry->size;
    }
    else {
      last_entry++;
      /* fill possible gap */
      if (last_entry != entry) *last_entry = *entry;
    }
  }

  num_entries = last_entry - result + 1;

  *count = num_entries;
  return result;
}

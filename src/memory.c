#include "debug.h"
#include "frames.h"
#include "memory.h"
#include "stage1.h"

#include <stdint.h>
#include <string.h>

#define FRAMES_MIN_ORDER 14 /* 16K frames */

frames_t *memory_frames;

typedef struct {
  uint64_t base;
  uint64_t size;
  uint64_t type;
} __attribute__((packed)) memory_map_entry_t;

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
map. */
chunk_t *memory_get_chunks(int *count, void *heap)
{
  regs16_t regs;

  memory_map_entry_t *entry0 = (memory_map_entry_t *)heap;
  memory_map_entry_t *entry = entry0;

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
      int num = entry - entry0;
      debug_byte(num >> 8);
      debug_byte(num);
      debug_str("\n");
      return 0;
    }

    kprintf("entry: 0x%x size: 0x%x type: %d\n",
            (unsigned long) entry->base,
            (unsigned long) entry->size,
            entry->type);
    entry++;
  } while (regs.ebx);

  int num_entries = entry - entry0;

  isort(entry0, num_entries, sizeof(memory_map_entry_t), &mm_compare);

  /* allocate an array of chunks */

  /* combine contiguous and overlapping entries */
  memory_map_entry_t *last_entry = entry0;

  chunk_t *chunk0 = (chunk_t *)entry;

  chunk_t *chunk = chunk0;
  chunk->base = last_entry->base;
  chunk->type = last_entry->type;
  chunk++;

  for (int i = 1; i < num_entries; i++) {
    memory_map_entry_t *entry = &entry0[i];

    /* overlap */
    if (entry->base < last_entry->base + last_entry->size) {
      unsigned int type = mm_type_combine(entry->type, last_entry->type);
      if (type != last_entry->type) {
        chunk->base = entry->base;
        chunk->type = type;
        chunk++;
      }

      if (entry->base + entry->size < last_entry->base + last_entry->size) {
        if (type != last_entry->type) {
          chunk->base = entry->base + entry->size;
          chunk->type = last_entry->type;
          chunk++;
        }
      }
      else {
        last_entry = entry;
      }
    }
    /* gap */
    else if (last_entry->base + last_entry->size < entry->base) {
      if (last_entry->type != MM_RESERVED) {
        chunk->base = last_entry->base + last_entry->size;
        chunk->type = MM_RESERVED;
        chunk++;
      }
      if (entry->type != MM_RESERVED) {
        chunk->base = entry->base;
        chunk->type = entry->type;
        chunk++;
      }
      last_entry = entry;
    }
    /* consecutive */
    else {
      if (last_entry->type != entry->type) {
        chunk->base = entry->base;
        chunk->type = entry->type;
        chunk++;
      }
      last_entry = entry;
    }
  }

  /* terminator chunk */
  if (last_entry->type != MM_RESERVED) {
    chunk->base = last_entry->base + last_entry->size;
    chunk->type = MM_RESERVED;
    chunk++;
  }

  int num_chunks = chunk - chunk0;

  /* discard memory map and compact memory */
  memmove(heap, chunk0, num_chunks * sizeof(chunk_t));
  *count = num_chunks;
  return (chunk_t *)heap;
}

int memory_add_chunk(chunk_t *chunks, int *num_chunks, uint64_t base)
{
  int i = 0;
  for (; i < *num_chunks; i++) {
    if (base == chunks[i].base) return i;
    if (base < chunks[i].base) {
      memmove(&chunks[i + 1], &chunks[i], (*num_chunks - i) * sizeof(chunk_t));
      break;
    }
  }
  chunks[i].base = base;
  chunks[i].type = i > 0 ? chunks[i - 1].type : MM_RESERVED;
  (*num_chunks)++;
  return i;
}

void memory_reserve_chunk(chunk_t *chunks, int *num_chunks,
                          uint64_t start, uint64_t end)
{
  if (start >= end) return;

  /* add chunks */
  int i = memory_add_chunk(chunks, num_chunks, start);
  int j = memory_add_chunk(chunks, num_chunks, end);

  for (int k = i; k < j; k++) {
    chunks[k].type = MM_RESERVED;
  }
}

int find_chunk(chunk_t *chunks, int num_chunks, uint64_t base)
{
  int i = 0;
  for (; i < num_chunks; i++) {
    if (base > chunks[i].base) break;
  }
  return i - 1;
}

typedef struct {
  chunk_t *chunks;
  int num_chunks;
} chunk_info_t;

/* return availability information for a memory block */
int mem_info(void *start, size_t size, void *data)
{
  chunk_info_t *chunk_info = data;

  int reserved = 0;
  int available = 0;

  int i = find_chunk(chunk_info->chunks, chunk_info->num_chunks,
                     (unsigned long) start);
  int j = find_chunk(chunk_info->chunks, chunk_info->num_chunks,
                     (unsigned long) (start + size));
  if (i < 0) {
    i = 0;
    reserved = 1;
  }

  for (int k = i; k <= j; k++) {
    if (chunk_info->chunks[k].type == MM_AVAILABLE) {
      available = 1;
    }
    else {
      reserved = 1;
    }
  }

  if (!available)
    return MEM_INFO_RESERVED;
  else if (!reserved)
    return MEM_INFO_USABLE;
  else
    return MEM_INFO_PARTIALLY_USABLE;

  return 0;
}

int memory_init(void *heap)
{
  int num_chunks;
  chunk_t *chunks = memory_get_chunks(&num_chunks, heap);
  if (!chunks || num_chunks <= 0) text_panic("memory");

  /* reserve high kernel memory */
  memory_reserve_chunk(chunks, &num_chunks,
                       (unsigned long)_kernel_start,
                       (unsigned long)_kernel_end);

  /* reserve BIOS and low kernel memory */
  heap += (num_chunks + 2) * sizeof(chunk_t);
  memory_reserve_chunk(chunks, &num_chunks,
                       0, (unsigned long) heap);

  /* There are potentially a few other memory areas that we might use,
  between 0x500 and 0x7c00, but note that the kernel stack is still at
  0x7b00 at this point, and the area around 0x2000 is used for v8086,
  so we just forget about any memory before the low kernel for
  simplicity. */

  debug_str("memory map:\n");
  for (int i = 0; i < num_chunks; i++) {
    debug_str("type: ");
    debug_byte(chunks[i].type);
    debug_str(" base: ");
    debug_byte(chunks[i].base >> 56); debug_byte(chunks[i].base >> 48);
    debug_byte(chunks[i].base >> 40); debug_byte(chunks[i].base >> 32);
    debug_byte(chunks[i].base >> 24); debug_byte(chunks[i].base >> 16);
    debug_byte(chunks[i].base >> 8); debug_byte(chunks[i].base);
    debug_str("\n");
  }

  uint64_t total_memory_size = chunks[num_chunks - 1].base;
  debug_str("memory size: ");
  debug_byte(total_memory_size >> 56); debug_byte(total_memory_size >> 48);
  debug_byte(total_memory_size >> 40); debug_byte(total_memory_size >> 32);
  debug_byte(total_memory_size >> 24); debug_byte(total_memory_size >> 16);
  debug_byte(total_memory_size >> 8); debug_byte(total_memory_size);
  debug_str("\n");

  chunk_info_t chunk_info;
  chunk_info.chunks = chunks;
  chunk_info.num_chunks = num_chunks;

  memory_frames = frames_new(0, FRAMES_MIN_ORDER, ORDER_OF(total_memory_size),
                             &mem_info, &chunk_info);

  uint32_t free_mem = frames_available_memory(memory_frames);
  debug_str("free memory: ");
  debug_byte(free_mem >> 24); debug_byte(free_mem >> 16);
  debug_byte(free_mem >> 8); debug_byte(free_mem);
  debug_str("\n");

  while(1);
  return 0;
}

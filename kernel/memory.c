#include "core/debug.h"
#include "core/v8086.h"
#include "buddy/frames.h"
#include "memory.h"

#include <stdint.h>
#include <string.h>

#define FRAMES_MIN_ORDER 14 /* 16K frames */

#define BIOS_MM_DEBUG 0

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

/* Get memory map from BIOS. Store chunks in the temporary heap */
chunk_t *memory_get_chunks(int *count, void **heap)
{
  regs16_t regs;

  memory_map_entry_t *entry0 = (memory_map_entry_t *)*heap;
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
#if BIOS_MM_DEBUG
      kprintf("memory map bios call failed\n  eax: %#x eflags: %#x\n",
              regs.eax, flags);
      int num = entry - entry0;
      kprintf("  num entries so far: %d\n", num);
#endif /* BIOS_MM_DEBUG */
      return 0;
    }

#if BIOS_MM_DEBUG
    kprintf("entry: %#016llx size: %#016llx type: %d\n",
            entry->base, entry->size, entry->type);
#endif /* BIOS_MM_DEBUG */
    entry++;
  } while (regs.ebx);

  int num_entries = entry - entry0;

  isort(entry0, num_entries, sizeof(memory_map_entry_t), &mm_compare);

  /* allocate an array of chunks */

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

  /* discard memory map and compact heap */
  memmove(*heap, chunk0, num_chunks * sizeof(chunk_t));
  *count = num_chunks;
  chunk_t *ret = *heap;
  *heap = ret + num_chunks;
  return ret;
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
    if (base < chunks[i].base) break;
  }
  return i - 1;
}

typedef struct {
  chunk_t *chunks;
  int num_chunks;
} chunk_info_t;

/* return availability information for a memory block */
int mem_info(void *startp, size_t size, void *data)
{
  chunk_info_t *chunk_info = data;
  int reserved = 0;
  int available = 0;

  uint64_t start = (uint32_t) startp;
  uint64_t length = size ? size : 1ULL << 32;

  /* kprintf("[mi] start = %#llx, length = %#llx\n", start, length); */

  int i = find_chunk(chunk_info->chunks, chunk_info->num_chunks, start);
  int j = find_chunk(chunk_info->chunks, chunk_info->num_chunks, start + length);
  if (i < 0) {
    i = 0;
    reserved = 1;
  }
  /* kprintf("[mi] i = %d, j = %d\n", i, j); */

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
  chunk_t *chunks = memory_get_chunks(&num_chunks, &heap);
  if (!chunks || num_chunks <= 0) panic();

  /* ignore low memory for now */
  memory_reserve_chunk(chunks, &num_chunks,
                       0, (unsigned long) _kernel_end);

  kprintf("memory map:\n");
  for (int i = 0; i < num_chunks; i++) {
    kprintf("type: %d base: %#016llx\n",
            chunks[i].type,
            chunks[i].base);
  }

  uint64_t total_memory_size = chunks[num_chunks - 1].base;
  if (total_memory_size > 0xffffffff)
    total_memory_size = 0xffffffff;
  kprintf("memory size: %#x\n", total_memory_size);

  chunk_info_t chunk_info;
  chunk_info.chunks = chunks;
  chunk_info.num_chunks = num_chunks;

  unsigned int max_order = ORDER_OF(total_memory_size);
  kprintf("max_order = %u\n", max_order);
  memory_frames = frames_new(0, FRAMES_MIN_ORDER, max_order,
                             &mem_info, &chunk_info);

  if (!memory_frames) {
    panic();
  }

  uint32_t free_mem = frames_available_memory(memory_frames);
  kprintf("free memory: %#x\n", free_mem);

  return 0;
}

void *falloc(size_t sz)
{
  return frames_alloc(memory_frames, sz);
}

void ffree(void *p)
{
  frames_free(memory_frames, p);
}
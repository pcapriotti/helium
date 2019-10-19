#include "core/debug.h"
#include "core/v8086.h"
#include "core/x86.h"
#include "frames.h"
#include "kmalloc.h"
#include "memory.h"
#include "multiboot.h"
#include "paging/paging.h"
#include "scheduler.h"

#include <stdint.h>
#include <string.h>

#define DMA_FRAMES_ORDER PAGE_BITS
#define KERNEL_FRAMES_ORDER 14 /* 16K frames */
#define USER_FRAMES_ORDER KERNEL_FRAMES_ORDER

#define BIOS_MM_DEBUG 0
#define MM_DEBUG 0

frames_t kernel_frames;
frames_t dma_frames;
frames_t user_frames;

typedef struct {
  uint64_t base;
  uint64_t size;
  uint64_t type;
} __attribute__((aligned(8), packed)) memory_map_entry_t;

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

static void _lock_frames(frames_t *frames)
{
  sched_disable_preemption();
}

static void _unlock_frames(frames_t *frames)
{
  sched_enable_preemption();
}

extern int v8086_tracing;

void get_memory_map(multiboot_t *multiboot, memory_map_entry_t *entry, int *num_entries)
{
  if (multiboot && multiboot->flags & MB_INFO_MMAP) {
    mb_mmap_entry_t *e = multiboot->mmap;
    mb_mmap_entry_t *e1 = (void *)multiboot->mmap +
      multiboot->mmap_length;

    int count = 0;
    while (e < e1) {
#if BIOS_MM_DEBUG
      kprintf("multiboot base: %#016llx length: %#016llx type: %d\n",
              e->base, e->length, e->type);
#endif
      entry->base = e->base;
      entry->size = e->length;
      entry->type = e->type;

      entry++;
      count++;
      e = (void *)e + e->size + 4;
    }

    *num_entries = count;
    return;
  }

  memory_map_entry_t *entry0 = entry;
  regs16_t regs;
  regs.ebx = 0;

  do {
    ptr16_t entry16 = linear_to_ptr16((uint32_t) entry);
    regs.eax = 0xe820;
    regs.edx = 0x534d4150;
    regs.es = entry16.segment;
    regs.edi = entry16.offset;
    regs.ecx = sizeof(memory_map_entry_t);
    entry->type = MM_AVAILABLE;


    int flags = bios_int(0x15, &regs);

    if (flags & EFLAGS_CF || regs.eax != 0x534d4150) {
#if BIOS_MM_DEBUG
      serial_printf("memory map bios call failed\n  eax: %#x eflags: %#x\n",
              regs.eax, flags);
      int num = entry - entry0;
      serial_printf("  num entries so far: %d\n", num);
#endif /* BIOS_MM_DEBUG */
      break;
    }

#if BIOS_MM_DEBUG
    serial_printf("entry: %#016llx size: %#016llx type: %d\n",
            entry->base, entry->size, entry->type);
#endif /* BIOS_MM_DEBUG */
    entry++;
  } while (regs.ebx);

  *num_entries = entry - entry0;
}

/* Get memory map from BIOS. Store chunks in the temporary heap */
chunk_t *memory_get_chunks(int *count, uint32_t **heap, multiboot_t *multiboot)
{
  regs16_t regs;

  memory_map_entry_t *entry0 = (memory_map_entry_t *)*heap;
  int num_entries = 0;

  get_memory_map(multiboot, entry0, &num_entries);

  isort(entry0, num_entries, sizeof(memory_map_entry_t), &mm_compare);

  /* allocate an array of chunks */

  memory_map_entry_t *last_entry = entry0;

  chunk_t *chunk0 = (chunk_t *) (entry0 + num_entries);

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
  chunk_t *ret = (chunk_t *)*heap;
  *heap = (void *)(ret + num_chunks);
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

  uint64_t start;
  uint64_t end;
} chunk_info_t;

/* return availability information for a memory block */
int mem_info(uint64_t start, uint64_t length, void *data)
{
  chunk_info_t *chunk_info = data;
  int reserved = 0;
  int available = 0;

  if (start + length < chunk_info->start) return MEM_INFO_RESERVED;
  if (start >= chunk_info->end) return MEM_INFO_RESERVED;

  if (start < chunk_info->start || start + length >= chunk_info->end)
    reserved = 1;

  int i = find_chunk(chunk_info->chunks, chunk_info->num_chunks, start);
  int j = find_chunk(chunk_info->chunks, chunk_info->num_chunks, start + length);
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

int chunk_info_init_frame(chunk_info_t *chunk_info,
                          frames_t *frames,
                          frames_t *aux_frames,
                          unsigned int order)
{
  int ret = frames_init(frames, aux_frames,
                        chunk_info->start, chunk_info->end,
                        order, &mem_info, chunk_info);
  if (ret == -1) return -1;

  frames->lock = _lock_frames;
  frames->unlock = _unlock_frames;

  return ret;
}

int memory_init(multiboot_t *multiboot)
{
  int num_chunks;

  uint32_t *heap = (uint32_t *) 0x20000; /* use some temporary low memory */

  chunk_t *chunks = memory_get_chunks(&num_chunks, &heap, multiboot);
  if (!chunks || num_chunks <= 0) panic();

  memory_reserve_chunk(chunks, &num_chunks,
                       (size_t)_kernel_start,
                       (size_t) _kernel_end);
  memory_reserve_chunk(chunks, &num_chunks,
                       0, 0x7c00);

#if MM_DEBUG
  serial_printf("memory map:\n");
  for (int i = 0; i < num_chunks; i++) {
    serial_printf("type: %d base: %#016llx\n",
                  chunks[i].type,
                  chunks[i].base);
  }
#endif
  chunk_info_t chunk_info;
  chunk_info.chunks = chunks;
  chunk_info.num_chunks = num_chunks;

  uint64_t total_memory_size = chunks[num_chunks - 1].base;
#if MM_DEBUG
  {
    serial_printf("memory size: %#llx (", total_memory_size);
    unsigned long long kb = total_memory_size / 1024;
    unsigned long long mb = kb / 1024;
    unsigned long long gb = mb / 1024;
    if (gb)
      serial_printf("%llu GB", gb);
    else if (mb)
      serial_printf("%llu MB", mb);
    else if (kb)
      serial_printf("%llu kB", kb);

    serial_printf(")\n");
  }
#endif

  /* create main kernel frame allocator */
  {
    chunk_info.start = (size_t) _kernel_start;
    chunk_info.end = KERNEL_MEMORY_END;

    if (chunk_info_init_frame(&chunk_info,
                              &kernel_frames, 0,
                              KERNEL_FRAMES_ORDER) == -1)
      return -1;
  }

#if MM_DEBUG
  serial_printf("initialising kernel heap\n");
#endif

  /* initialise kernel heap */
  if (kmalloc_init() == -1) panic();

  /* move chunks to kernel memory, otherwise they might be overwritten
     by the DMA frame allocator */
  {
    size_t chunk_size = num_chunks * sizeof(chunk_t);
    chunk_info.chunks = kmalloc(chunk_size);
    memcpy(chunk_info.chunks, chunks, chunk_size);
  }

#if MM_DEBUG
  serial_printf("creating DMA allocator\n");
#endif

  /* create DMA frame allocator */
  {
    chunk_info.start = 0;
    chunk_info.end = (size_t) _kernel_start;

    if (chunk_info_init_frame(&chunk_info,
                              &dma_frames, &kernel_frames,
                              DMA_FRAMES_ORDER) == -1)
      return -1;
  }

#if MM_DEBUG
  serial_printf("enabling paging\n");
#endif

  /* enable paging now, because the user allocator will need it */
  if (paging_init(total_memory_size) == -1) panic();

#if MM_DEBUG
  serial_printf("creating user allocator\n");
#endif

  /* TODO: use kernel allocator for metadata */
  if (total_memory_size > USER_MEMORY_START) {
    chunk_info.start = USER_MEMORY_START;
    chunk_info.end = total_memory_size;
    uint64_t max = paging_maximum_memory();
    if (max && chunk_info.end > max) {
      chunk_info.end = max;
    }

    if (chunk_info_init_frame(&chunk_info,
                              &user_frames, &kernel_frames,
                              USER_FRAMES_ORDER) == -1)
      return -1;
  }

  return 0;
}

void *falloc(size_t sz)
{
  uint64_t frame = frames_alloc(&kernel_frames, sz);
  assert(frame < KERNEL_MEMORY_END);
  return (void *) (size_t) frame;
}

void ffree(void *p)
{
  uint64_t frame = (size_t) p;
  assert(frame < KERNEL_MEMORY_END);
  frames_free(&kernel_frames, frame);
}

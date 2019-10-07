#include "core/debug.h"
#include "multiboot.h"

void mb_print_mmap(multiboot_t *multiboot)
{
  if (!(multiboot->flags & MB_INFO_MMAP)) return;
  mb_mmap_entry_t *e = multiboot->mmap;
  mb_mmap_entry_t *e1 = (void *)multiboot->mmap +
    multiboot->mmap_length;

  while (e < e1) {
    kprintf("base: %#016llx length: %#016llx type: %d\n",
            e->base, e->length, e->type);
    e = (void *)e + e->size + 4;
  }
}

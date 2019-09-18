#include "console.h"
#include "debug.h"
#include "graphics.h"
#include "memory.h"
#include "stage1.h"

#include <stdint.h>
#include <stddef.h>

void main()
{
  void *heap = _kernel_low_end;

  int num_chunks;
  chunk_t *chunks = memory_get_chunks(&num_chunks, heap);

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

  if (!chunks || num_chunks <= 0) text_panic("memory");
  heap += num_chunks + sizeof(chunk_t);

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

  while(1) {}

  {
    vbe_mode_t mode;
    mode.width = 800;
    mode.height = 600;
    mode.bpp = 32;
    if (graphics_init(&mode) == -1) text_panic("graphics");
  }

  if (console_init() == -1) panic();

  /* TODO: allocate memory for the console */
  console_set_buffer((uint16_t *) heap);
  console_print_str("Ok.", 7);

  __asm__ volatile("hlt");
  while(1);
}

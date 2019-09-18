#include "console.h"
#include "debug.h"
#include "graphics.h"
#include "memory.h"
#include "stage1.h"

#include <stdint.h>
#include <stddef.h>

void main()
{
  vbe_mode_t mode;
  mode.width = 800;
  mode.height = 600;
  mode.bpp = 32;

  size_t mm_size;
  memory_map_entry_t *mm = get_memory_map(&mm_size);
  if (!mm || mm_size <= 0) text_panic("memory");

  debug_str("memory map:\n");
  for (unsigned int i = 0; i < mm_size; i++) {
    debug_str("type: ");
    debug_byte(mm[i].type);
    debug_str(" base: ");
    debug_byte(mm[i].base >> 56); debug_byte(mm[i].base >> 48);
    debug_byte(mm[i].base >> 40); debug_byte(mm[i].base >> 32);
    debug_byte(mm[i].base >> 24); debug_byte(mm[i].base >> 16);
    debug_byte(mm[i].base >> 8); debug_byte(mm[i].base);
    debug_str(" size: ");
    debug_byte(mm[i].size >> 56); debug_byte(mm[i].size >> 48);
    debug_byte(mm[i].size >> 40); debug_byte(mm[i].size >> 32);
    debug_byte(mm[i].size >> 24); debug_byte(mm[i].size >> 16);
    debug_byte(mm[i].size >> 8); debug_byte(mm[i].size);
    debug_str("\n");
  }
  while(1);

  if (graphics_init(&mode) == -1) text_panic("graphics");
  if (console_init() == -1) panic();

  /* TODO: allocate memory for the console */
  console_set_buffer((uint16_t *) 0x180000);
  console_print_str("Ok.", 7);

  __asm__ volatile("hlt");
  while(1);
}

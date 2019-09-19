#include "console.h"
#include "debug.h"
#include "graphics.h"
#include "memory.h"
#include "stage1.h"

#include <stdint.h>
#include <stddef.h>

void main()
{
  __asm__ volatile("sti");

  void *heap = _kernel_low_end;

  if (memory_init(heap) == -1) panic();

  {
    vbe_mode_t mode;
    mode.width = 800;
    mode.height = 600;
    mode.bpp = 32;
    if (graphics_init(&mode) == -1) text_panic("graphics");
  }

  if (console_init() == -1) panic();
  kprintf("console %dx%d\n",
          console.width, console.height);

  __asm__ volatile("hlt");
  while(1);
}

#include "debug.h"
#include "graphics.h"
#include "stdint.h"

void panic(void)
{
  uint32_t *fb = (uint32_t *)graphics_mode.framebuffer + 400;
  fb[0] = 0x00ff0000;
  __asm__ volatile("hlt");
  while(1);
}

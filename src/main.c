#include "stdint.h"

void main()
{
  uint16_t *vga_text = (uint16_t *)0xb8000;
  vga_text[0] = 0x2000;
  __asm__ volatile("hlt");
  while(1);
}

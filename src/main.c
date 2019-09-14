#include "stdint.h"

void main()
{
  uint16_t *vga_text = (uint16_t *)0xb8000;
  vga_text[0] = 0x700 | 'O';
  vga_text[1] = 0x700 | 'k';

  __asm__ volatile("hlt");
  while(1);
}

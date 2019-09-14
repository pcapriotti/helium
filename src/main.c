#include "graphics.h"
#include "stdint.h"
#include "gdt.h"

void print_num(int num, int base)
{
  for (int i = 16; i >= 0; i--) {
    int x = num % base;
    char digit;
    if (x > 9) {
      digit = 'a' + x - 10;
    }
    else {
      digit = '0' + x;
    }
    vga_text[i] = 0x0700 | digit;
    num /= base;
  }
}

void main()
{
  vbe_mode_t mode;
  mode.width = 1024;
  mode.height = 768;
  mode.bpp = 16;

  int ret = graphics_init(&mode);
  if (ret == -1) panic();

  print_num((uint32_t) mode.number, 16);

  __asm__ volatile("hlt");
  while(1);
}

#include "console.h"
#include "debug.h"
#include "graphics.h"
#include "stdint.h"
#include "stage1.h"

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
  mode.width = 800;
  mode.height = 600;
  mode.bpp = 32;

  if (graphics_init(&mode) == -1) text_panic();
  if (console_init() == -1) panic();

  int colour = 0x00808080;
  console_print_char(console_at(0, 0), 'O', colour);
  console_print_char(console_at(1, 0), 'k', colour);
  console_print_char(console_at(2, 0), '.', colour);

  __asm__ volatile("hlt");
  while(1);
}

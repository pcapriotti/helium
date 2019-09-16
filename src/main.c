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

  /* TODO: allocate memory for the console */
  console_set_buffer((uint16_t *) 0x180000);
  console_print_str("Ok.", 7);

  __asm__ volatile("hlt");
  while(1);
}

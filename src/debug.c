#include "debug.h"
#include "graphics.h"
#include "stdint.h"

#include <stdarg.h>

void panic(void)
{
  uint32_t *fb = (uint32_t *)graphics_mode.framebuffer + 400;
  fb[0] = 0x00ff0000;
  __asm__ volatile("hlt");
  while(1);
}

#define VGA_TEXT ((volatile uint16_t*) 0xb8000)

volatile uint16_t *vga_text = VGA_TEXT;

struct {
  int x, y;
  volatile uint16_t *p;
} debug_console = {0, 0, VGA_TEXT};

void print_char(char c)
{
  if (c == '\n') {
    debug_console.p += 80 - debug_console.x;
    debug_console.y++;
    debug_console.x = 0;
  }
  else if (debug_console.x < 80) {
    *debug_console.p++ = 0x700 | c;
    debug_console.x++;
  }
  if (debug_console.y == 25) {
    for (int i = 0; i < 80 * 24; i++) {
      vga_text[i] = vga_text[i + 80];
    }
    for (int i = 0; i < 80; i++) {
      vga_text[80 * 24 + i] = 0;
    }
    debug_console.y--;
  }
}

void print_digit(uint8_t d)
{
  if (d < 10) {
    print_char(d + '0');
  }
  else {
    print_char(d + 'a' - 10);
  }
}

int print_uint(long n, unsigned int base)
{
  uint8_t str[256];

  /* number of digits */
  int digits = 0;
  {
    long m = n;
    while (m) {
      m /= base;
      digits++;
    }
  }
  if (digits == 0) digits = 1;

  for (int i = digits - 1; i >= 0; i--) {
    str[i] = n % base;
    n /= base;
  }

  for (int i = 0; i < digits; i++) {
    print_digit(str[i]);
  }

  return digits;
}

static inline int print_int(long n, int base)
{
  if (n < 0) {
    print_char('-');
    return print_uint(-n, base);
  }
  else {
    return print_uint(n, base);
  }
}

int kvprintf(const char *fmt, va_list list)
{
  int count = 0;
  for (int i = 0; fmt[i]; i++) {
    if (fmt[i] == '%') {
      i++;
      switch (fmt[i]) {
      case '\0':
        return count;
      case '%':
        print_char('%');
        count++;
        break;
      case 'd':
        {
          int n = va_arg(list, int);
          count += print_int(n, 10);
        }
        break;
      case 'x':
        {
          unsigned long n = va_arg(list, unsigned long);
          count += print_uint(n, 16);
        }
        break;
      }
    }
    else {
      print_char(fmt[i]);
    }
  }

  return 0;
}

int kprintf(const char *fmt, ...)
{
  va_list list;
  va_start(list, fmt);

  int ret = kvprintf(fmt, list);

  va_end(list);

  return ret;
}

void debug_str(const char *s)
{
  char c;
  while ((c = *s++)) {
    print_char(c);
  }
}

void debug_byte(uint8_t x)
{
  print_digit(x >> 4);
  print_digit(x & 0xf);
}

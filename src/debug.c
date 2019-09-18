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

int isdigit(char c)
{
  return c >= '0' && c <= '9';
}

#ifdef SERIAL_PORT_DEBUG
#include "io.h"
void print_char(char c)
{
  if (c == '\n')
    outb(0x3f8, '\r');
  outb(0x3f8, c);
}
#else
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
    debug_console.p -= 80;
    debug_console.y--;
  }
}
#endif

int print_string(const char *s)
{
  char c;
  int i = 0;
  while ((c = *s++)) {
    print_char(c);
    i++;
  }
  return i;
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

void print_digit_X(uint8_t d)
{
  if (d < 10) {
    print_char(d + '0');
  }
  else {
    print_char(d + 'A' - 10);
  }
}

int print_uint(long n, unsigned int base, int X, int alt, int padded, int width)
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

  if (!padded) {
    for (int i = digits; i < width; i++) {
      print_char(' ');
    }
  }
  if (alt) {
    switch (base) {
    case 16:
      print_char('0');
      print_char('x');
      break;
    case 8:
      print_char('0');
      break;
    }
  }
  if (padded) {
    for (int i = digits; i < width; i++) {
      print_char('0');
    }
  }
  for (int i = 0; i < digits; i++) {
    if (X)
      print_digit_X(str[i]);
    else
      print_digit(str[i]);
  }

  return digits;
}

static inline int print_int(long n, int base, int X, int alt, int padded, int width)
{
  if (n < 0) {
    print_char('-');
    return print_uint(-n, base, X, alt, padded, width);
  }
  else {
    return print_uint(n, base, X, alt, padded, width);
  }
}

int kvprintf(const char *fmt, va_list list)
{
  int count = 0;
  for (int i = 0; fmt[i]; i++) {
    if (fmt[i] == '%') {
      i++;

      int alt = 0;
      int padded = 0;

      /* flags */
      int done = 0;
      while (!done) {
        switch (fmt[i]) {
        case '#':
          alt = 1;
          i++;
          break;
        case '0':
          padded = 1;
          i++;
          break;
        default:
          done = 1;
          break;
        }
      }

      /* width */
      int width = 0;
      done = 0;
      while (!done) {
        if (isdigit(fmt[i])) {
          width *= 10;
          width += fmt[i] - '0';
          i++;
        }
        else {
          done = 1;
        }
      }

      /* length */
      done = 0;
      while (!done) {
        switch (fmt[i]) {
        case 'l':
          i++;
          break;
        default:
          done = 1;
          break;
        }
      }

      switch (fmt[i]) {
      case '\0':
        return count;
      case '%':
        print_char('%');
        count++;
        break;
      case 'c':
        {
          char c = va_arg(list, int);
          print_char(c);
          count++;
        }
        break;
      case 's':
        {
          const char *s = va_arg(list, const char *);
          count += print_string(s);
        }
        break;
      case 'd':
      case 'i':
        {
          int n = va_arg(list, int);
          count += print_int(n, 10, 0, alt, padded, width);
        }
        break;
      case 'u':
        {
          unsigned int n = va_arg(list, unsigned int);
          count += print_uint(n, 10, 0, alt, padded, width);
        }
        break;
      case 'o':
        {
          unsigned int n = va_arg(list, unsigned int);
          count += print_uint(n, 8, 0, alt, padded, width);
        }
        break;
      case 'p':
        alt = 1; __attribute__ ((fallthrough));
      case 'x':
        {
          unsigned int n = va_arg(list, unsigned int);
          count += print_uint(n, 16, 0, alt, padded, width);
        }
        break;
      case 'X':
        {
          unsigned int n = va_arg(list, unsigned int);
          count += print_uint(n, 16, 1, alt, padded, width);
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
  print_string(s);
}

void debug_byte(uint8_t x)
{
  print_digit(x >> 4);
  print_digit(x & 0xf);
}

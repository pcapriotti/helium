#include "debug.h"
#include "io.h"
#include "serial.h"
#include "stacktrace.h"
#include "x86.h"

#include <math.h>

#define SERIAL_DEBUG 1

volatile int debug_key_pressed = 0;
int debug_paging = 0;

void hang_system(void) {
  while(1) {
    __asm__ ("hlt");
  }
}

void _panic(const char *filename, int line)
{
  __asm__ volatile("cli");
  int col = serial_set_colour(SERIAL_COLOUR_ERR);
  serial_printf("kernel panic: %s:%d\n", filename, line);
  serial_printf("  flags: %#x\n", cpu_flags());
  serial_set_colour(col);
  /* stacktrace_print_current(); */
  hang_system();
}

#define VGA_TEXT ((volatile uint16_t*) 0xb8000)

volatile uint16_t *vga_text = VGA_TEXT;
debug_console_t debug_console = {0, 0, VGA_TEXT};

#define DEBUG_CONSOLE_WIDTH 80
#define DEBUG_CONSOLE_HEIGHT 25

void debug_print_char(char c)
{
  if (c == '\n') {
    debug_console.p += DEBUG_CONSOLE_WIDTH - debug_console.x;
    debug_console.y++;
    debug_console.x = 0;
  }
  else if (debug_console.x < DEBUG_CONSOLE_WIDTH) {
    *debug_console.p++ = 0x700 | c;
    debug_console.x++;
  }
  if (debug_console.y == DEBUG_CONSOLE_HEIGHT) {
    if (debug_paging) {
      while (!debug_key_pressed) {
        __asm__("hlt");
      }
      debug_key_pressed = 0;
    }
    for (int i = 0; i < DEBUG_CONSOLE_WIDTH * (DEBUG_CONSOLE_HEIGHT - 1); i++) {
      vga_text[i] = vga_text[i + DEBUG_CONSOLE_WIDTH];
    }
    for (int i = 0; i < DEBUG_CONSOLE_WIDTH; i++) {
      vga_text[DEBUG_CONSOLE_WIDTH * (DEBUG_CONSOLE_HEIGHT - 1) + i] = 0;
    }
    debug_console.p -= DEBUG_CONSOLE_WIDTH;
    debug_console.y--;
  }
}

void serial_print_char(char c)
{
#if SERIAL_DEBUG
  serial_putchar(c);
#else
  print_char_function(c);
  redraw_screen_function();
#endif
}

void noop(void) {}

void (*print_char_function)(char c) = debug_print_char;
void (*redraw_screen_function)(void) = noop;

int isdigit(char c)
{
  return c >= '0' && c <= '9';
}

int print_string(print_char_t print_char, const char *s)
{
  char c;
  int i = 0;
  while ((c = *s++)) {
    print_char(c);
    i++;
  }
  return i;
}

void print_digit(print_char_t print_char, uint8_t d)
{
  if (d < 10) {
    print_char(d + '0');
  }
  else {
    print_char(d + 'a' - 10);
  }
}

void print_digit_X(print_char_t print_char, uint8_t d)
{
  if (d < 10) {
    print_char(d + '0');
  }
  else {
    print_char(d + 'A' - 10);
  }
}

int print_uint(print_char_t print_char, uintmax_t n, unsigned int base,
               int X, int alt, int padded, int width)
{
  uint8_t str[256];

  /* number of digits */
  int digits = 0;
  {
    uint64_t m = n;
    while (m) {
      m = div64sd(m, base);
      digits++;
    }
  }
  if (digits == 0) digits = 1;

  for (int i = digits - 1; i >= 0; i--) {
    str[i] = mod64sd(n, base);
    n = div64sd(n, base);
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
      print_digit_X(print_char, str[i]);
    else
      print_digit(print_char, str[i]);
  }

  return digits;
}

static inline int print_int(print_char_t print_char, intmax_t n, int base,
                            int X, int alt, int padded, int width)
{
  if (n < 0) {
    print_char('-');
    return print_uint(print_char, -n, base, X, alt, padded, width);
  }
  else {
    return print_uint(print_char, n, base, X, alt, padded, width);
  }
}

int kvprintf(print_char_t print_char, const char *fmt, va_list list)
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
      int length = 0;
      while (!done) {
        switch (fmt[i]) {
        case 'l':
          i++;
          length++;
          break;
        default:
          done = 1;
          break;
        }
      }

      int base = 16;
      switch (fmt[i]) {
      case '\0':
        break;
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
          count += print_string(print_char, s);
        }
        break;
      case 'd':
      case 'i':
        {
          intmax_t n;
          if (length > 1) {
            n = va_arg(list, long long int);
          }
          else if (length == 1) {
            n = va_arg(list, long int);
          }
          else {
            n = va_arg(list, int);
          }
          count += print_int(print_char, n, 10, 0, alt, padded, width);
        }
        break;
      case 'o':
        base -= 2; __attribute__ ((fallthrough));
      case 'u':
        base -= 6; __attribute__ ((fallthrough));
      case 'p':
        alt = 1; __attribute__ ((fallthrough));
      case 'x':
      case 'X':
        {
          uintmax_t n;
          if (length > 1) {
            n = va_arg(list, long long unsigned int);
          }
          else if (length == 1) {
            n = va_arg(list, long unsigned int);
          }
          else {
            n = va_arg(list, unsigned int);
          }
          count += print_uint(print_char, n, base, 0, alt, padded, width);
        }
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

  int ret = kvprintf(print_char_function, fmt, list);

  va_end(list);

  return ret;
}

void debug_str(const char *s)
{
  print_string(serial_print_char, s);
}

void debug_byte(uint8_t x)
{
  print_digit(serial_print_char, x >> 4);
  print_digit(serial_print_char, x & 0xf);
}

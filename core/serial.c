#include "debug.h"
#include "io.h"
#include "serial.h"

#include <stdarg.h>

#define SERIAL_COLOURS 1

#ifdef SERIAL_COLOURS
static int serial_colour = SERIAL_COLOUR_MSG;
#endif /* SERIAL_COLOURS */

void serial_init(void) {
  outb(COM1_PORT + SERIAL_INTERRUPT_ENABLE, 0);
  outb(COM1_PORT + SERIAL_LINE_CONTROL, SERIAL_DLAB);
  outb(COM1_PORT + SERIAL_DIVISOR_LO, 1);
  outb(COM1_PORT + SERIAL_DIVISOR_HI, 0);
  outb(COM1_PORT + SERIAL_LINE_CONTROL,
       SERIAL_8_BITS | SERIAL_NO_PARITY | SERIAL_1_STOP);
  outb(COM1_PORT + SERIAL_FIFO,
       SERIAL_FIFO_ENABLE |
       SERIAL_FIFO_CLEAR_RX |
       SERIAL_FIFO_CLEAR_TX |
       SERIAL_FIFO_TRIGGER_14);
  outb(COM1_PORT + SERIAL_MODEM_CONTROL,
       SERIAL_MODEM_DTR |
       SERIAL_MODEM_RTS |
       SERIAL_MODEM_OUT2);
}

void _serial_putchar(char c)
{
  while ((inb(COM1_PORT + SERIAL_LINE_STATUS) &
          SERIAL_STATUS_TX_EMPTY) == 0);
  outb(COM1_PORT, c);
}

void serial_newline(void)
{
  _serial_putchar('\r');
  _serial_putchar('\n');
}

void serial_putchar(char c)
{
  if (c == '\n' || c == '\r')
    serial_newline();
  else
    _serial_putchar(c);
}

int serial_set_colour(int col)
{
#if SERIAL_COLOURS
  int old = serial_colour;
  serial_colour = col;
  return old;
#else
  return col;
#endif
}

#if SERIAL_COLOURS
void serial_colour_escape(int col)
{
  _serial_putchar(0x1b);
  _serial_putchar('[');
  if (col) {
    print_digit(_serial_putchar, (col >> 4) & 0xf);
    print_digit(_serial_putchar, col & 0xf);
  }
  else {
    _serial_putchar('0');
  }
  _serial_putchar('m');
}
#endif

int serial_printf(const char *fmt, ...)
{
  va_list list;
  va_start(list, fmt);

#if SERIAL_COLOURS
  serial_colour_escape(serial_colour);
#endif
  int ret = kvprintf(serial_print_char, fmt, list);
#if SERIAL_COLOURS
  serial_colour_escape(0);
#endif

  va_end(list);

  return ret;
}

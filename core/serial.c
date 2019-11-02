#include "io.h"
#include "serial.h"

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

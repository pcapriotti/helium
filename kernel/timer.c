#include "io.h"
#include "timer.h"

#define PIT_FREQ 1193182

typedef struct {
  unsigned long count;
} timer_t;

static timer_t timer;

void timer_send_command(uint8_t cmd, uint16_t data)
{
  if (data < 0x100) {
    outb(PIT_CMD, cmd | PIT_ACCESS_LO);
    outb(PIT_DATA0, data);
  }
  else if ((data & 0xff) == 0) {
    outb(PIT_CMD, cmd | PIT_ACCESS_HI);
    outb(PIT_DATA0, data >> 8);
  }
  else {
    outb(PIT_CMD, cmd | PIT_ACCESS_LOHI);
    outb(PIT_DATA0, data);
    outb(PIT_DATA0, data >> 8);
  }
}

void timer_set_divider(uint16_t d)
{
  timer_send_command(PIT_MODE_SQUARE_WAVE, d);
}

int timer_init(void)
{
  timer_set_divider(PIT_FREQ / 1000);
  return 0;
}

void timer_irq(void)
{
  timer.count++;
}

unsigned long timer_get_tick(void)
{
  return timer.count;
}

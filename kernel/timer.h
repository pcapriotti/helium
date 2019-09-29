#ifndef timer_H
#define TIMER_H

/* ports */
enum {
  PIT_DATA0 = 0x40,
  PIT_CMD = 0x43,
};

/* binary / bcd */
enum {
  PIT_BCD_OFF = 0,
  PIT_BCD_ON = 1 << 0,
};

/* operating mode */
enum {
  PIT_MODE_INTERRUPT = 0,
  PIT_MODE_ONE_SHOT = 1 << 1,
  PIT_MODE_RATE_GEN = 2 << 1,
  PIT_MODE_SQUARE_WAVE = 3 << 1,
  PIT_MODE_SW_STROBE = 4 << 1,
  PIT_MODE_HW_STROBE = 5 << 1,
};

/* access mode */
enum {
  PIT_ACCESS_LATCH = 0,
  PIT_ACCESS_LO = 1 << 4,
  PIT_ACCESS_HI = 2 << 4,
  PIT_ACCESS_LOHI = 3 << 4,
};

/* channel */
enum {
  PIT_CHANNEL0 = 0,
  PIT_CHANNEL1 = 1 << 6,
  PIT_CHANNEL2 = 2 << 6,
  PIT_READ_BACK = 3 << 6,
};

void timer_set_divider(uint16_t d);

/* set frequency in Hz */
void timer_set_freq(unsigned int freq);

int timer_init(void);

/* handle IRQ */
void timer_irq(void);

unsigned long timer_get_tick(void);

void timer_sleep(unsigned long delay);

#endif /* TIMER_H */

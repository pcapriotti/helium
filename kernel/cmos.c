#define CMOS_CMD 0x70
#define CMOS_DATA 0x71

#include "core/debug.h"
#include "core/io.h"
#include "datetime.h"

enum {
  CMOS_NMI_DISABLE = 1 << 7,
};

enum {
  CMOS_SECONDS = 0x00,
  CMOS_MINUTES = 0x02,
  CMOS_HOURS = 0x04,
  CMOS_DAY = 0x07,
  CMOS_MONTH = 0x08,
  CMOS_YEAR = 0x09,
  CMOS_STATUS_A = 0x0a,
  CMOS_STATUS_B = 0x0b,
};

enum {
  CMOS_STATUS_24_HOUR = 1 << 1,
  CMOS_STATUS_BINARY = 1 << 2,
};

static int cmos_nmi_disable = 0;

uint8_t cmos_read(unsigned reg)
{
  unsigned nmi = cmos_nmi_disable ? CMOS_NMI_DISABLE : 0;
  outb(CMOS_CMD, (reg & 0x7f) | nmi);
  return inb(CMOS_DATA);
}

uint8_t cmos_bcd(uint8_t value, uint8_t status)
{
  if (status & CMOS_STATUS_BINARY)
    return value;
  else
    return (value & 0xf) + (value >> 4) * 10;
}

uint8_t cmos_24_hour(uint8_t value, uint8_t status)
{
  if (status & CMOS_STATUS_24_HOUR) {
    return cmos_bcd(value, status);
  }
  else {
    unsigned pm = (value & 0x7f) >> 7;
    return (cmos_bcd(value & 0x7f, status) % 12) + pm * 12;
  }
}

uint8_t cmos_read_time_value(unsigned reg, uint8_t status)
{
  return cmos_bcd(cmos_read(reg), status);
}

uint8_t cmos_read_hour(unsigned reg, uint8_t status)
{
  return cmos_24_hour(cmos_read(reg), status);
}

void cmos_get_datetime(datetime_t *dt)
{
  uint8_t status = cmos_read(CMOS_STATUS_B);
  dt->time.seconds = cmos_read_time_value(CMOS_SECONDS, status);
  dt->time.minutes = cmos_read_time_value(CMOS_MINUTES, status);
  dt->time.hours = cmos_read_hour(CMOS_HOURS, status);
  dt->date.day = cmos_read_time_value(CMOS_DAY, status);
  dt->date.month = cmos_read_time_value(CMOS_MONTH, status);

  /* guess century */
  int year = cmos_read_time_value(CMOS_YEAR, status);
  if (year < CURRENT_YEAR % 100) {
    year += 100;
  }
  dt->date.year = year + CURRENT_YEAR - (CURRENT_YEAR % 100);
}

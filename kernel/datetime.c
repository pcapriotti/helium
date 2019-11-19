#include "core/debug.h"
#include "datetime.h"

void datetime_debug(datetime_t *dt)
{
  serial_printf("%u-%02u-%02u %02u:%02u:%02u",
                dt->date.year,
                dt->date.month,
                dt->date.day,
                dt->time.hours,
                dt->time.minutes,
                dt->time.seconds);
}

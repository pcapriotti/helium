#include "pci.h"
#include "drivers/drivers.h"

#include <stdint.h>

#define DEBUG_LOCAL 1

typedef struct rtl8169 {
  uint16_t iobase;
  uint8_t irq;
} rtl8169_t;

rtl8169_t rtl8169_instance;

int rtl8169_matches(void *data, device_t *dev)
{
  return dev->id == 0x816810ec;
}

int rtl8169_init(void *data, device_t *dev)
{
  rtl8169_t *rtl = data;
#if DEBUG_LOCAL
  serial_printf("[rtl8169] init\n");
#endif
  return 0;
}

driver_t rtl8169_driver = {
  .matches = rtl8169_matches,
  .init = rtl8169_init,
  .data = &rtl8169_instance,
};

#include "core/debug.h"
#include "drivers/drivers.h"
#include "drivers/rtl8139/driver.h"
#include "pci.h"

#define LOCAL_DEBUG 1

typedef struct {
  uint16_t iobase;
} data_t;

uint16_t iobase(device_t *dev)
{
  for (int i = 0; i < PCI_NUM_BARS; i++) {
    if (dev->bars[i] && dev->bars[i] & 1) {
      return dev->bars[i] & ~3;
    }
  }

  return 0;
}

int rtl8139_init(void *_data, device_t *dev)
{
  data_t *data = _data;

  data->iobase = iobase(dev);

#if LOCAL_DEBUG
  kprintf("[rtl8139] irq: %u\n", (unsigned int) dev->irq);
  kprintf("[rtl8139] iobase: %#x\n", data->iobase);
#endif
  return 0;
}

int rtl8139_matches(void *data, device_t *dev)
{
  return dev->id == 0x813910ec;
}

data_t rtl8139_data = {0};
driver_t rtl8139_driver = {
  .matches = rtl8139_matches,
  .init = rtl8139_init,
  .data = &rtl8139_data,
};

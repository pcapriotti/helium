#include "core/debug.h"
#include "drivers/drivers.h"
#include "drivers/rtl8139/driver.h"
#include "pci.h"

#define LOCAL_DEBUG 1

int rtl8139_init(void *data, device_t *dev)
{
#if LOCAL_DEBUG
  kprintf("rtl8139 init\n");
#endif
  return 0;
}

int rtl8139_matches(void *data, device_t *dev)
{
  return dev->id == 0x813910ec;
}

driver_t rtl8139_driver = {
  .matches = rtl8139_matches,
  .init = rtl8139_init,
  .data = 0
};

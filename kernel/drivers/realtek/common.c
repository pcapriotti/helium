#include "drivers/realtek/common.h"
#include "pci.h"

uint16_t rtl_find_iobase(device_t *dev)
{
  for (int i = 0; i < PCI_NUM_BARS; i++) {
    if (dev->bars[i] && dev->bars[i] & 1) {
      return dev->bars[i] & ~3;
    }
  }

  return 0;
}

#include "debug.h"
#include "io.h"

#define PCI_CONF_ADDR 0xcf8
#define PCI_CONF_DATA 0xcfc

#define PCI_DEBUG 1

enum {
  PCI_VENDOR_DEVICE = 0,
  PCI_STATUS_COMMAND,
  PCI_CLASS,
  PCI_HEADER_TYPE,
  PCI_BAR0,
};

enum {
  PCI_CLS_UNCLASSIF = 0,
  PCI_CLS_STORAGE,
  PCI_CLS_NETWORK,
  PCI_CLS_DISPLAY,
  PCI_CLS_MULTIMEDIA,
  PCI_CLS_MEMORY,
  PCI_CLS_BRIDGE,
  PCI_CLS_COMM,
  PCI_CLS_SYS,
  PCI_CLS_INPUT,
  PCI_CLS_DOCKING,
  PCI_CLS_CPU,
  PCI_CLS_SERIAL,
  PCI_CLS_WIFI
};

uint32_t pci_read(uint8_t bus, uint8_t device,
                  uint8_t func, uint8_t offset)
{
  uint32_t address =
    (offset << 2) |
    ((uint32_t)(func & 7) << 8) |
    ((uint32_t)(device & 0x1f) << 11) |
    ((uint32_t)bus << 16) |
    1 << 31;

  outl(PCI_CONF_ADDR, address);
  return inl(PCI_CONF_DATA);
}

void pci_check_function(uint8_t bus, uint8_t device, uint8_t func)
{
  uint32_t cl = pci_read(bus, device, func, PCI_CLASS);
  uint32_t hd = pci_read(bus, device, func, PCI_HEADER_TYPE);
  uint8_t htype = (hd >> 16) & 0x7f;
  if (htype == 0) { /* general device */
    /* uint8_t revision = cl & 0xff; */
    /* uint8_t prog_if = (cl >> 8) & 0xff; */
    uint8_t subclass = (cl >> 16) & 0xff;
    uint8_t class = (cl >> 24) & 0xff;
    if (class == PCI_CLS_STORAGE &&
        subclass == 0x01) { /* IDE controller */
#if PCI_DEBUG
      uint32_t vd = pci_read(bus, device, func, PCI_VENDOR_DEVICE);
      kprintf("found IDE controller: bus %u no %u vd %#x cl %#x\n",
              bus, device, vd, cl);
#endif
      uint32_t bars[4];
      for (unsigned int i = 0; i < 4; i++) {
        bars[i] = pci_read(bus, device, func, PCI_BAR0 + i) & ~3;
      }
      for (int i = 0; i < 4; i++) {
        kprintf("bar%d: %p\n", i, bars[i]);
      }
    }
  }
}

void pci_check_device(uint8_t bus, uint8_t device)
{
  uint32_t vd = pci_read(bus, device, 0, PCI_VENDOR_DEVICE);
  if (vd == (uint32_t)(~0)) {
    return;
  }
  uint32_t hd = pci_read(bus, device, 0, PCI_HEADER_TYPE);
  kprintf("found device bus %u dev %u hd %#x\n",
          bus, device, hd);
  pci_check_function(bus, device, 0);
  if ((hd >> 23) & 1) { /* multifunction */
    for (uint8_t func = 1; func < 8; func++) {
      pci_check_function(bus, device, func);
    }
  }
}

void pci_scan(void)
{
  for (unsigned int bus = 0; bus < 0x100; bus++) {
    for (unsigned int device = 0; device < 0x20; device++) {
      pci_check_device(bus, device);
    }
  }
}

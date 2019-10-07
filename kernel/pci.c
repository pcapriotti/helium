#include "core/debug.h"
#include "core/io.h"
#include "kmalloc.h"
#include "pci.h"

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
  PCI_H1_BUS_NUMBER = 6,
  PCI_H1_STATUS = 6,
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

list_t *pci_check_function(uint8_t bus, uint8_t device, uint8_t func)
{
  uint32_t cl = pci_read(bus, device, func, PCI_CLASS);
  uint32_t hd = pci_read(bus, device, func, PCI_HEADER_TYPE);
  uint8_t subclass = (cl >> 16) & 0xff;
  uint8_t class = (cl >> 24) & 0xff;
  uint8_t htype = (hd >> 16) & 0x7f;
  switch (htype) {
  case 0: /* general device */
    /* uint8_t revision = cl & 0xff; */
    /* uint8_t prog_if = (cl >> 8) & 0xff; */
    {
#if PCI_DEBUG
      uint32_t vd = pci_read(bus, device, func, PCI_VENDOR_DEVICE);
      kprintf("found device: bus %u no %u vd %#x cl %#x\n",
              bus, device, vd, cl);
#endif
      device_t *dev = kmalloc(sizeof(device_t));
      dev->class = class;
      dev->subclass = subclass;
      for (unsigned int i = 0; i < 6; i++) {
        dev->bars[i] = pci_read(bus, device, func, PCI_BAR0 + i) & ~3;
      }
#if PCI_DEBUG
      for (int i = 0; i < 6; i++) {
        if (dev->bars[i])
          kprintf("  bar%d: %p\n", i, dev->bars[i]);
      }
#endif
      dev->head.next = &dev->head;
      dev->head.prev = &dev->head;
      return &dev->head;
    }
    break;
  case 1: /* bridge */
    if (class == PCI_CLS_BRIDGE &&
        subclass == PCI_BRIDGE_PCI) {
      uint32_t val = pci_read(bus, device, func, PCI_H1_STATUS);
      uint8_t bus = (val >> 8) & 0xff;
      return pci_scan_bus(bus);
    }
    break;
  }

  return 0;
}

list_t *pci_check_device(uint8_t bus, uint8_t id)
{
  uint32_t vd = pci_read(bus, id, 0, PCI_VENDOR_DEVICE);
  if (vd == (uint32_t)(~0)) {
    return 0;
  }
  uint32_t hd = pci_read(bus, id, 0, PCI_HEADER_TYPE);
#if PCI_DEBUG
  kprintf("found device bus %u id %u hd %#x\n",
          bus, id, hd);
#endif
  list_t *ret = pci_check_function(bus, id, 0);
  if ((hd >> 23) & 1) { /* multifunction */
    for (uint8_t func = 1; func < 8; func++) {
      list_splice(&ret, pci_check_function(bus, id, func));
    }
  }
  return ret;
}

list_t *pci_scan_bus(uint8_t bus)
{
  list_t *ret = 0;
  for (unsigned int id = 0; id < 0x20; id++) {
    list_splice(&ret, pci_check_device(bus, id));
  }
  return ret;
}

list_t *pci_scan()
{
  list_t *ret = pci_scan_bus(0);
#if PCI_DEBUG
  {
    list_t *p = ret ? ret->next : 0;
    while (p != ret) {
      device_t *dev = DEV_LIST_ENTRY(p);
      kprintf("found device %p class: %#x subclass: %#x\n",
              dev, dev->class, dev->subclass);
    }
  }
#endif
  return ret;
}

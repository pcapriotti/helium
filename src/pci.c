#include "debug.h"
#include "io.h"
#include "kmalloc.h"
#include "pci.h"

#define PCI_CONF_ADDR 0xcf8
#define PCI_CONF_DATA 0xcfc

#define PCI_DEBUG 0

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

enum {
  PCI_STORAGE_SCSI = 0,
  PCI_STORAGE_IDE,
  PCI_STORAGE_FLOPPY,
  PCI_STORAGE_IPI,
  PCI_STORAGE_RAID,
  PCI_STORAGE_ATA,
  PCI_STORAGE_SATA,
  PCI_STORAGE_SSCSI,
  PCI_STORAGE_NV,
  PCI_STORAGE_OTHER = 0x80,
};

enum {
  PCI_BRIDGE_HOST = 0,
  PCI_BRIDGE_ISA,
  PCI_BRIDGE_EISA,
  PCI_BRIDGE_MCA,
  PCI_BRIDGE_PCI,
  PCI_BRIDGE_PCMCIA,
  PCI_BRIDGE_NUBUS,
  PCI_BRIDGE_CARDBUS,
  PCI_BRIDGE_PCI2,
  PCI_BRIDGE_INFINIBAND,
  PCI_BRIDGE_OTHER = 0x80,
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

void pci_scan_bus(list_t *devices, uint8_t bus);

void pci_check_function(list_t *devices, uint8_t bus,
                        uint8_t device, uint8_t func)
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
    if (class == PCI_CLS_STORAGE &&
        subclass == PCI_STORAGE_IDE) { /* IDE controller */
#if PCI_DEBUG
      uint32_t vd = pci_read(bus, device, func, PCI_VENDOR_DEVICE);
      kprintf("found IDE controller: bus %u no %u vd %#x cl %#x\n",
              bus, device, vd, cl);
#endif
      device_t *dev = kmalloc(sizeof(device_t));
      dev->class = class;
      dev->subclass = subclass;
      for (unsigned int i = 0; i < 4; i++) {
        dev->bars[i] = pci_read(bus, device, func, PCI_BAR0 + i) & ~3;
      }
      list_add(&dev->head, devices);
#if PCI_DEBUG
      for (int i = 0; i < 4; i++) {
        kprintf("  bar%d: %p\n", i, dev->bars[i]);
      }
#endif
    }
    break;
  case 1: /* bridge */
    if (class == PCI_CLS_BRIDGE &&
        subclass == PCI_BRIDGE_PCI) {
      uint32_t val = pci_read(bus, device, func, PCI_H1_STATUS);
      uint8_t bus = (val >> 8) & 0xff;
      pci_scan_bus(devices, bus);
    }
    break;
  }
}

void pci_check_device(list_t *devices, uint8_t bus, uint8_t id)
{
  uint32_t vd = pci_read(bus, id, 0, PCI_VENDOR_DEVICE);
  if (vd == (uint32_t)(~0)) {
    return;
  }
  uint32_t hd = pci_read(bus, id, 0, PCI_HEADER_TYPE);
#if PCI_DEBUG
  kprintf("found device bus %u id %u hd %#x\n",
          bus, id, hd);
#endif
  pci_check_function(devices, bus, id, 0);
  if ((hd >> 23) & 1) { /* multifunction */
    for (uint8_t func = 1; func < 8; func++) {
      pci_check_function(devices, bus, id, func);
    }
  }
}

void pci_scan_bus(list_t *devices, uint8_t bus)
{
  for (unsigned int id = 0; id < 0x20; id++) {
    pci_check_device(devices, bus, id);
  }
}

void pci_scan(list_t *devices)
{
  pci_scan_bus(devices, 0);
}

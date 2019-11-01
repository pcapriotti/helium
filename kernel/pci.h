#ifndef PCI_H
#define PCI_H

#include "list.h"

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

extern const char *pci_class_names[];

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

extern const char *pci_storage_names[];

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

extern const char *pci_bridge_names[];

enum {
  PCI_CMD_IO = (1 << 0),
  PCI_CMD_MEM = (1 << 1),
  PCI_CMD_BUS_MASTER = (1 << 2),
  PCI_CMD_IRQ_DISABLE = (1 << 10),
};

typedef struct device {
  list_t head;
  uint32_t bars[6];
  uint8_t bus;
  uint8_t device;
  uint8_t func;
  uint8_t class;
  uint8_t subclass;
  uint32_t id;
  uint8_t irq;
} device_t;

void device_command_set_mask(device_t *dev, uint16_t mask);

#define DEV_LIST_ENTRY(x) LIST_ENTRY(x, device_t, head)
#define PCI_NUM_BARS 6

struct driver;

list_t *pci_scan();
list_t *pci_scan_bus(uint8_t bus);
void pci_add_driver(struct driver *drv);

#endif /* PCI_H */

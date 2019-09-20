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


typedef struct {
  list_t head;
  uint32_t bars[4];
  uint8_t class;
  uint8_t subclass;
} device_t;

void pci_scan(list_t *devices);

#endif /* PCI_H */

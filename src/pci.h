#ifndef PCI_H
#define PCI_H

#include "list.h"

typedef struct {
  list_t head;
  uint32_t bars[4];
  uint8_t class;
  uint8_t subclass;
} device_t;

void pci_scan(list_t *devices);

#endif /* PCI_H */

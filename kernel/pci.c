#include "core/debug.h"
#include "core/io.h"
#include "drivers/drivers.h"
#include "drivers/ata/ata.h"
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
  PCI_H0_IRQ = 0xf,
};

#define PCI_MAX_DRIVERS 64
driver_t pci_drivers[PCI_MAX_DRIVERS];
size_t pci_num_drivers = 0;

static inline uint32_t pci_address(uint8_t bus, uint8_t device,
                                   uint8_t func, uint8_t offset)
{
  return (offset << 2) |
    ((uint32_t)(func & 7) << 8) |
    ((uint32_t)(device & 0x1f) << 11) |
    ((uint32_t)bus << 16) |
    1 << 31;
}

uint32_t pci_read(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset)
{
  uint32_t address = pci_address(bus, device, func, offset);
  outl(PCI_CONF_ADDR, address);
  return inl(PCI_CONF_DATA);
}

void pci_write(uint8_t bus, uint8_t device,
               uint8_t func, uint8_t offset,
               uint32_t value)
{
  uint32_t address = pci_address(bus, device, func, offset);
  outl(PCI_CONF_ADDR, address);
  outl(PCI_CONF_DATA, value);
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
      uint32_t id = pci_read(bus, device, func, PCI_VENDOR_DEVICE);
      uint32_t irq = pci_read(bus, device, func, PCI_H0_IRQ);
      device_t *dev = kmalloc(sizeof(device_t));
      dev->bus = bus;
      dev->device = device;
      dev->func = func;
      dev->class = class;
      dev->subclass = subclass;
      dev->id = id;
      dev->irq = irq & 0xff;
#if PCI_DEBUG
      serial_printf("found device: bus %u no %u id %#x cl %#x\n",
              bus, device, id, cl);
      serial_printf("  irq: %08x\n", irq);
#endif
      for (unsigned int i = 0; i < PCI_NUM_BARS; i++) {
        dev->bars[i] = pci_read(bus, device, func, PCI_BAR0 + i);
      }
#if PCI_DEBUG
      for (int i = 0; i < PCI_NUM_BARS; i++) {
        if (dev->bars[i])
          serial_printf("  bar%d: %p\n", i, dev->bars[i]);
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
  serial_printf("found device bus %u id %u hd %#x\n",
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

void pci_device_init(device_t *dev)
{
  for (size_t i = 0; i < pci_num_drivers; i++) {
    driver_t *driver = &pci_drivers[i];
    if (driver->matches(driver->data, dev)) {
      driver->init(driver->data, dev);
      return;
    }
  }
}

list_t *pci_scan()
{
  list_t *ret = pci_scan_bus(0);
  {
    list_t *p = ret ? ret->next : 0;
    while (p != ret) {
      device_t *dev = DEV_LIST_ENTRY(p);
#if PCI_DEBUG
      serial_printf("found device %p class: %#x subclass: %#x\n",
              dev, dev->class, dev->subclass);
#endif
      pci_device_init(dev);
      p = p->next;
    }
  }
  return ret;
}

void pci_add_driver(driver_t *drv)
{
  if (pci_num_drivers >= PCI_MAX_DRIVERS) {
    serial_printf("too many drivers\n");
    panic();
  }

  pci_drivers[pci_num_drivers++] = *drv;
}

void device_command_set_mask(device_t *dev, uint16_t mask)
{
  uint32_t sc = pci_read(dev->bus, dev->device, dev->func, PCI_STATUS_COMMAND);
  pci_write(dev->bus, dev->device, dev->func, PCI_STATUS_COMMAND, sc | mask);
}

#include "drivers.h"
#include "drivers/ata/ata.h"
#include "drivers/rtl8139/driver.h"
#include "pci.h"

void drivers_init(void)
{
  pci_add_driver(&ata_driver);
  pci_add_driver(&rtl8139_driver);
}

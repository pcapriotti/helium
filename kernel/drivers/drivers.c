#include "drivers.h"
#include "drivers/ata/ata.h"
#include "pci.h"

void drivers_init(void)
{
  pci_add_driver(&ata_driver);
}

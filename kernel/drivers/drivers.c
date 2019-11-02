#include "drivers.h"
#include "drivers/ata/ata.h"
#include "drivers/realtek/rtl8139.h"
#include "drivers/realtek/rtl8169.h"
#include "pci.h"

void drivers_init(void)
{
  pci_add_driver(&ata_driver);
  pci_add_driver(&rtl8139_driver);
  pci_add_driver(&rtl8169_driver);
}

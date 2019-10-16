#ifndef DRIVER_H
#define DRIVER_H

struct driver;
extern struct driver rtl8139_driver;

void rtl8139_irq(void);

#endif /* DRIVER_H */

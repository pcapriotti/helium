#ifndef DRIVER_H
#define DRIVER_H

struct driver;
extern struct driver rtl8139_driver;

struct nic_ops;
extern struct nic_ops rtl8139_ops;
extern struct nic rtl8139_nic;

#endif /* DRIVER_H */

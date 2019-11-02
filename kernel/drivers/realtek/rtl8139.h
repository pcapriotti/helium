#ifndef DRIVERS_REALTEK_RTL8139_H
#define DRIVERS_REALTEK_RTL8139_H

struct driver;
extern struct driver rtl8139_driver;

struct nic_ops;
extern struct nic_ops rtl8139_ops;
extern struct nic rtl8139_nic;

#endif /* DRIVERS_REALTEK_RTL8139_H */

#ifndef DRIVER_H
#define DRIVER_H

struct driver;
extern struct driver rtl8139_driver;

struct nic_ops;
extern struct nic_ops rtl8139_ops;
extern void *rtl8139_ops_data;

#endif /* DRIVER_H */

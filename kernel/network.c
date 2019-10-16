#include "drivers/rtl8139/driver.h"
#include "core/debug.h"
#include "network.h"

void dispatch(void *data, uint8_t *payload, size_t size)
{
  serial_printf("[network] packet received, size = %u\n", size);
}

void start_network(nic_ops_t *ops, void *ops_data)
{
  ops->grab(ops_data, dispatch, 0);
}

void network_init(void)
{
  /* just use rtl8139 for now */
  /* TODO: use a device manager */
  start_network(&rtl8139_ops, rtl8139_ops_data);
}

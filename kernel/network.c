#include "arp.h"
#include "arpa/inet.h"
#include "drivers/rtl8139/driver.h"
#include "core/debug.h"
#include "network.h"

#define DEBUG_LOCAL 1

typedef struct eth_frame {
  mac_t destination;
  mac_t source;
  uint16_t type;
  uint8_t payload[];
} __attribute__((packed)) eth_frame_t;

uint16_t eth_frame_type(eth_frame_t *frame)
{
  return ntohs(frame->type);
}

void dispatch(void *data, uint8_t *payload, size_t size)
{
  eth_frame_t *frame = (eth_frame_t *) payload;

#if DEBUG_LOCAL
  serial_printf("[network] packet received, size = %u\n", size);
  serial_printf("[network] ");
  debug_mac(frame->source);
  serial_printf(" => ");
  debug_mac(frame->destination);
  serial_printf("\n");
#endif

  uint16_t type = eth_frame_type(frame);
  switch (type) {
  case ETYPE_ARP:
    {
      arp_receive_packet(frame->payload, size - sizeof(eth_frame_t));
    }
    break;
  case ETYPE_IPV6:
  case ETYPE_IPV4:
#if DEBUG_LOCAL
    serial_printf("[network] IP protocol not implemented yet\n", type);
#endif
    break;
  default:
#if DEBUG_LOCAL
    serial_printf("[network] unknown ethernet frame type %#04x\n", type);
#endif
    break;
  }
}

void start_network(nic_ops_t *ops, void *ops_data)
{
  arp_init();
  ops->grab(ops_data, dispatch, 0);
}

void network_init(void)
{
  /* just use rtl8139 for now */
  /* TODO: use a device manager */
  start_network(&rtl8139_ops, rtl8139_ops_data);
}

void debug_mac(mac_t mac)
{
  for (int i = 0; i < 6; i++) {
    serial_printf("%s%02x", i == 0 ? "" : ":", mac[i]);
  }
}

void debug_ipv4(ipv4_t ip)
{
  for (int i = 0; i < 4; i++) {
    serial_printf("%s%u", i == 0 ? "" : ".", (ip >> (i * 8)) & 0xff);
  }
}

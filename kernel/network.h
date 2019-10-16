#ifndef NETWORK_H
#define NETWORK_H

#include <stddef.h>
#include <stdint.h>

enum {
  ETYPE_IPV4 = 0x0800,
  ETYPE_IPV6 = 0x86dd,
  ETYPE_ARP = 0x0806,
};

typedef void (*nic_on_packet_t)(void *data,
                                uint8_t *payload,
                                size_t size);

typedef struct nic_ops {
  int (*grab)(void *data,
              nic_on_packet_t on_packet,
              void *on_packet_data);
} nic_ops_t;

void network_init(void);

typedef uint8_t mac_t[6];
typedef uint32_t ipv4_t;

void debug_mac(mac_t mac);
void debug_ipv4(ipv4_t ip);

#endif /* NETWORK_H */

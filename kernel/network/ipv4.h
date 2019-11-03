#ifndef IPV4_H
#define IPV4_H

#include "network/types.h"

#include <stddef.h>

struct nic;

#define IP_DEFAULT_TTL 64

enum {
  IP_PROTO_ICMP = 0x1,
  IP_PROTO_IGMP = 0x2,
  IP_PROTO_TCP = 0x6,
  IP_PROTO_UDP = 0x11,
};

enum {
  IP_ERR_TOO_LARGE,
  IP_ERR_ARP_MISS,
};

int ipv4_receive_packet(struct nic *nic, void *payload, size_t size);
void *ipv4_packet_new(int flags,
                      struct nic *nic,
                      uint8_t protocol,
                      size_t payload_size,
                      ipv4_t destination,
                      int *error);
int ipv4_transmit(struct nic *nic, void *payload, size_t size);

#endif /* IPV4_H */

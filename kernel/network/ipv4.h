#ifndef IPV4_H
#define IPV4_H

#include <stddef.h>

struct nic;

enum {
  IP_PROTO_ICMP = 0x1,
  IP_PROTO_IGMP = 0x2,
  IP_PROTO_TCP = 0x6,
  IP_PROTO_UDP = 0x11,
};

int ipv4_receive_packet(struct nic *nic, void *payload, size_t size);

#endif /* IPV4_H */

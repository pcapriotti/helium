#ifndef NETWORK_UDP_H
#define NETWORK_UDP_H

#include "network/types.h"

#include <stddef.h>

struct nic;

int udp_receive_packet(struct nic *nic, ipv4_t source,
                       void *packet, size_t size);

#endif /* NETWORK_UDP_H */

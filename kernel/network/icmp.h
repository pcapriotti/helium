#ifndef NETWORK_ICMP_H
#define NETWORK_ICMP_H

#include "network/types.h"

#include <stddef.h>

struct nic;

int icmp_receive_packet(struct nic *nic, ipv4_t source,
                        void *packet, size_t size);

#endif /* NETWORK_ICMP_H */

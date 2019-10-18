#ifndef NETWORK_ICMP_H
#define NETWORK_ICMP_H

#include <stddef.h>

struct nic;

int icmp_receive_packet(struct nic *nic, void *packet, size_t size);

#endif /* NETWORK_ICMP_H */

#ifndef IPV4_H
#define IPV4_H

#include <stddef.h>

struct nic;

int ipv4_receive_packet(struct nic *nic, void *payload, size_t size);

#endif /* IPV4_H */

#ifndef NETWORK_UDP_H
#define NETWORK_UDP_H

#include "network/types.h"

#include <stddef.h>

struct nic;

int udp_receive_packet(struct nic *nic, ipv4_t source,
                       void *packet, size_t size);

typedef void (*udp_on_packet_t)(void* data,
                                struct nic *nic,
                                ipv4_t src,
                                uint16_t src_port,
                                void *payload,
                                size_t size);

int udp_grab_port(uint16_t port,
                  udp_on_packet_t on_packet,
                  void *on_packet_data);

#endif /* NETWORK_UDP_H */

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

void *udp_packet_new(int flags, struct nic *nic,
                     uint16_t src_port,
                     ipv4_t dst,
                     uint16_t dst_port,
                     size_t payload_size,
                     int *error);

int udp_transmit(struct nic *nic, void *payload, size_t size);

#endif /* NETWORK_UDP_H */

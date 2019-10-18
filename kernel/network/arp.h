#ifndef ARP_H
#define ARP_H

#include <stdint.h>
#include <stddef.h>

struct nic;

void arp_receive_packet(struct nic *nic, uint8_t *payload, size_t size);

#endif /* ARP_H */

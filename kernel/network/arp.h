#ifndef ARP_H
#define ARP_H

#include <stdint.h>
#include <stddef.h>

struct nic;

void arp_receive_packet(uint8_t *payload, size_t size);
void arp_init(struct nic *nic);

#endif /* ARP_H */

#ifndef ARP_H
#define ARP_H

#include "network/types.h"

struct nic;

void arp_receive_packet(struct nic *nic, uint8_t *payload, size_t size);
int arp_resolve(ipv4_t ip, mac_t *mac);

#endif /* ARP_H */

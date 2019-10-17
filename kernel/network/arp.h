#ifndef ARP_H
#define ARP_H

#include <stdint.h>
#include <stddef.h>

struct nic_ops;

void arp_receive_packet(uint8_t *payload, size_t size);
void arp_init(struct nic_ops *ops, void *ops_data);

#endif /* ARP_H */

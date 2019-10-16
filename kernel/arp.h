#ifndef ARP_H
#define ARP_H

#include <stdint.h>
#include <stddef.h>

void arp_receive_packet(uint8_t *payload, size_t size);
void arp_init(void);

#endif /* ARP_H */

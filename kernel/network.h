#ifndef NETWORK_H
#define NETWORK_H

#include <stddef.h>
#include <stdint.h>

typedef void (*nic_on_packet_t)(void *data,
                                uint8_t *payload,
                                size_t size);

typedef struct nic_ops {
  int (*grab)(void *data,
              nic_on_packet_t on_packet,
              void *on_packet_data);
} nic_ops_t;

void network_init(void);

#endif /* NETWORK_H */

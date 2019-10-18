#ifndef NETWORK_H
#define NETWORK_H

#include "list.h"
#include "network/types.h"

struct heap;
struct nic;

typedef struct eth_frame {
  mac_t destination;
  mac_t source;
  uint16_t type;
  uint8_t payload[];
} __attribute__((packed)) eth_frame_t;

typedef void (*nic_on_packet_t)(void *data,
                                struct nic *nic,
                                uint8_t *payload,
                                size_t size);

typedef struct nic_ops {
  int (*grab)(void *data,
              nic_on_packet_t on_packet,
              void *on_packet_data);
  int (*transmit)(void *data, void *buf, size_t len);
  mac_t (*mac)(void *data);
} nic_ops_t;

typedef struct nic {
  list_t head;
  void *ops_data;
  nic_ops_t *ops;
  ipv4_t ip;
  const char *name;
} nic_t;

enum {
  /* reserved for the main task */
  ETH_FRAME_STATIC,
};

/* allocate a new frame */
eth_frame_t *eth_frame_alloc(int flags, size_t payload_size);

/* initialise a preallocated frame, return pointer to payload */
void *eth_frame_init(nic_t *nic,
                     eth_frame_t *frame,
                     uint16_t type,
                     mac_t destination);

/* transmit a packet: frame must be initialised and have space for
padding and CRC */
int eth_transmit(nic_t *ops, void *payload, size_t length);

void network_init(void);
struct heap *network_get_heap(void);

void debug_mac(mac_t mac);
void debug_ipv4(ipv4_t ip);

uint32_t crc32(uint8_t *buf, size_t size);
uint32_t crc_sum(uint8_t *buf, size_t size, uint32_t crc);

#endif /* NETWORK_H */

#ifndef NETWORK_H
#define NETWORK_H

#include <stddef.h>
#include <stdint.h>

#define ETH_MTU 1500
#define ETH_MIN_PAYLOAD_SIZE 46

enum {
  ETYPE_IPV4 = 0x0800,
  ETYPE_IPV6 = 0x86dd,
  ETYPE_ARP = 0x0806,
};

typedef struct mac {
  uint8_t data[6];
} __attribute__((packed)) mac_t;
typedef uint32_t ipv4_t;

typedef struct eth_frame {
  mac_t destination;
  mac_t source;
  uint16_t type;
  uint8_t payload[];
} __attribute__((packed)) eth_frame_t;

typedef void (*nic_on_packet_t)(void *data,
                                uint8_t *payload,
                                size_t size);

typedef struct nic_ops {
  int (*grab)(void *data,
              nic_on_packet_t on_packet,
              void *on_packet_data);
  int (*transmit)(void *data, void *buf, size_t len);
  mac_t (*mac)(void *data);
} nic_ops_t;

void network_init(void);

/* transmit a packet: frame must contain an ethernet header and have
space for padding and CRC */
int network_transmit(nic_ops_t *ops, void *ops_data,
                     eth_frame_t *frame, size_t payload_size);

void debug_mac(mac_t mac);
void debug_ipv4(ipv4_t ip);

uint32_t crc32(uint8_t *buf, size_t size);
uint32_t crc_sum(uint8_t *buf, size_t size, uint32_t crc);

#endif /* NETWORK_H */

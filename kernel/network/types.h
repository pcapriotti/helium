#ifndef NETWORK_TYPES_H
#define NETWORK_TYPES_H

#include <stdint.h>
#include <stddef.h>

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

#endif /* NETWORK_TYPES_H */

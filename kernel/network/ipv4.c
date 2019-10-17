#include "core/debug.h"
#include "ipv4.h"
#include "network.h"

#include <arpa/inet.h>
#include <stdint.h>

#define DEBUG_LOCAL 1

enum {
  PROTO_ICMP = 0x1,
  PROTO_IGMP = 0x2,
  PROTO_TCP = 0x6,
  PROTO_UDP = 0x11,
};

typedef struct ipv4_header {
  uint8_t version_ihl;
  uint8_t dscp_ecn;
  uint16_t length;
  uint16_t ident;
  uint16_t flags_fragment;
  uint8_t ttl;
  uint8_t protocol;
  uint16_t checksum;
  ipv4_t source_ip;
  ipv4_t destination_ip;
  uint32_t options[0];
} __attribute__((packed)) ipv4_header_t;

uint8_t ipv4_header_version(ipv4_header_t *header)
{
  return header->version_ihl >> 4;
}

uint8_t ipv4_header_ihl(ipv4_header_t *header)
{
  return header->version_ihl & 0xf;
}

uint16_t ipv4_header_length(ipv4_header_t *header)
{
  return ntohs(header->length);
}

int ipv4_receive_packet(void *packet, size_t size)
{
  ipv4_header_t *header = packet;
  if (ipv4_header_version(header) != 4) return -1;

  void *payload = header->options + ipv4_header_ihl(header) - 5;
#if DEBUG_LOCAL
  serial_printf("[ipv4] packet prococol %#x, data:\n", header->protocol);
  for (size_t i = 0; i < ipv4_header_length(header); i++) {
    serial_printf("%02x ", ((uint8_t *) payload)[i]);
  }
  serial_printf("\n");
#endif

  return 0;
}

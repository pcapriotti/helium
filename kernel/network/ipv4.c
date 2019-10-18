#include "core/debug.h"
#include "network/arp.h"
#include "network/icmp.h"
#include "network/ipv4.h"
#include "network.h"

#include <arpa/inet.h>
#include <stdint.h>

#define DEBUG_LOCAL 1

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

void ipv4_header_set_version_ihl(ipv4_header_t *header,
                                 uint8_t version, uint8_t ihl)
{
  header->version_ihl = (version << 4) | (ihl & 0xf);
}

uint16_t ipv4_header_length(ipv4_header_t *header)
{
  return ntohs(header->length);
}

int ipv4_receive_packet(nic_t *nic, void *packet, size_t size)
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

  if (header->destination_ip != nic->ip) return -1;

  switch (header->protocol) {
  case IP_PROTO_ICMP:
    icmp_receive_packet(nic, header->source_ip,
                        payload, ipv4_header_length(header));
    break;
  default:
    break;
  }

  return 0;
}

uint16_t checksum16(void *data, size_t length)
{
  uint16_t ret = 0;
  uint16_t *p = data;
  length = length >> 1;
  while (length--) {
    ret += *p++;
  }
  return ~ret;
}

void *ipv4_packet_new(int flags, nic_t *nic,
                      uint8_t protocol,
                      size_t payload_size,
                      ipv4_t destination,
                      int *error)
{
  size_t length = payload_size + sizeof(ipv4_header_t);

  mac_t destination_mac;
  if (arp_resolve(destination, &destination_mac) == -1) {
    *error = IP_ERR_ARP_MISS;
    return 0;
  }

  eth_frame_t *frame = eth_frame_alloc(flags, length);
  if (!frame) {
    *error = IP_ERR_TOO_LARGE;
    return 0;
  }

  ipv4_header_t *header = eth_frame_init(nic, frame,
                                         ETYPE_IPV4,
                                         destination_mac);

  ipv4_header_set_version_ihl(header, 4, 5);
  header->dscp_ecn = 0;
  header->length = length;
  header->ident = 0;
  header->flags_fragment = 0;
  header->ttl = IP_DEFAULT_TTL;
  header->protocol = protocol;
  header->source_ip = nic->ip;
  header->destination_ip = destination;
  header->checksum = 0;
  header->checksum = checksum16(header, sizeof(ipv4_header_t));

  *error = 0;
  return (void *) header + ipv4_header_length(header);
}

int ipv4_transmit(nic_t *nic, void *payload, size_t size)
{
  ipv4_header_t *header = payload - sizeof(ipv4_header_t);

  return eth_transmit(nic, header, size + sizeof(ipv4_header_t));
}

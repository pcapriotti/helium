#include "arpa/inet.h"
#include "arp.h"
#include "core/debug.h"
#include "network.h"

#define DEBUG_LOCAL 1

typedef struct arp_packet {
  uint16_t htype;
  uint16_t ptype;
  uint8_t hlen;
  uint8_t plen;
  uint16_t operation;
  uint8_t sender_mac[6];
  uint32_t sender_ip;
  uint8_t target_mac[6];
  uint32_t target_ip;
} __attribute__((packed)) arp_packet_t;

enum {
  OP_REQUEST = 1,
  OP_REPLY = 2,
};

uint16_t arp_packet_htype(arp_packet_t *packet)
{
  return ntohs(packet->htype);
}

uint16_t arp_packet_ptype(arp_packet_t *packet)
{
  return ntohs(packet->ptype);
}

uint16_t arp_packet_operation(arp_packet_t *packet)
{
  return ntohs(packet->operation);
}

void arp_receive_packet(uint8_t *payload, size_t size)
{
  arp_packet_t *packet = (arp_packet_t *) payload;

  if (arp_packet_htype(packet) != 1) return;
  if (arp_packet_ptype(packet) != ETYPE_IPV4) return;
  if (packet->hlen != 6) return;
  if (packet->plen != 4) return;

  uint16_t op = arp_packet_operation(packet);
  switch (op) {
  case OP_REQUEST:
#if DEBUG_LOCAL
    serial_printf("[arp] req about ");
    debug_ipv4(packet->target_ip);
    serial_printf(" from ");
    debug_ipv4(packet->sender_ip);
    serial_printf(" (");
    debug_mac(packet->sender_mac);
    serial_printf(")\n");
#endif
    break;
  case OP_REPLY:
    break;
  default:
#if DEBUG_LOCAL
    serial_printf("[arp] unknown operation %u\n", op);
#endif
    break;
  }
}

#include "arpa/inet.h"
#include "arp.h"
#include "core/debug.h"
#include "network.h"
#include "scheduler.h"
#include "semaphore.h"

#include <string.h>

#define DEBUG_LOCAL 1

#define ARP_BUFSIZE 64

typedef struct arp_packet {
  uint16_t htype;
  uint16_t ptype;
  uint8_t hlen;
  uint8_t plen;
  uint16_t operation;
  mac_t sender_mac;
  ipv4_t sender_ip;
  mac_t target_mac;
  ipv4_t target_ip;
} __attribute__((packed)) arp_packet_t;

enum {
  OP_REQUEST = 1,
  OP_REPLY = 2,
};

static uint8_t packet_buffer[ETH_MTU];

uint16_t arp_packet_htype(arp_packet_t *packet)
{
  return ntohs(packet->htype);
}

void arp_packet_set_htype(arp_packet_t *packet, uint16_t htype)
{
  packet->htype = htons(htype);
}

uint16_t arp_packet_ptype(arp_packet_t *packet)
{
  return ntohs(packet->ptype);
}

void arp_packet_set_ptype(arp_packet_t *packet, uint16_t ptype)
{
  packet->ptype = htons(ptype);
}

uint16_t arp_packet_operation(arp_packet_t *packet)
{
  return ntohs(packet->operation);
}

void arp_packet_set_operation(arp_packet_t *packet, uint16_t operation)
{
  packet->operation = htons(operation);
}

int process_packet(nic_t *nic, arp_packet_t *packet)
{
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
    serial_printf("), ip = %#x\n", packet->target_ip);
#endif

    if (packet->target_ip == nic->ip) {
      /* reply with our mac address */
      eth_frame_t *frame = (eth_frame_t *) packet_buffer;
      arp_packet_t *reply = eth_frame_init(nic, frame,
                                           ETYPE_ARP,
                                           packet->sender_mac);

      reply->target_mac = packet->sender_mac;
      reply->target_ip = packet->sender_ip;

      reply->sender_mac = nic->ops->mac(nic->ops_data);
      reply->sender_ip = nic->ip;
      reply->hlen = 6;
      reply->plen = 4;
      arp_packet_set_htype(reply, 1);
      arp_packet_set_ptype(reply, ETYPE_IPV4);
      arp_packet_set_operation(reply, OP_REPLY);

      eth_transmit(nic, frame, sizeof(arp_packet_t));
    }
    break;
  case OP_REPLY:
    break;
  default:
#if DEBUG_LOCAL
    serial_printf("[arp] unknown operation %u\n", op);
#endif
    return -1;
  }

  return 0;
}

void arp_receive_packet(nic_t *nic, uint8_t *payload, size_t size)
{
  arp_packet_t *packet = (arp_packet_t *) payload;

  if (arp_packet_htype(packet) != 1) return;
  if (arp_packet_ptype(packet) != ETYPE_IPV4) return;
  if (packet->hlen != 6) return;
  if (packet->plen != 4) return;

  process_packet(nic, packet);
}

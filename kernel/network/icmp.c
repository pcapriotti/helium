#include "core/debug.h"
#include "network/icmp.h"
#include "network/ipv4.h"
#include "network/network.h"

#include <string.h>

#define DEBUG_LOCAL 0

typedef struct icmp_packet {
  uint8_t type, code;
  uint16_t checksum;
  uint32_t payload[0];
} __attribute__((packed)) icmp_packet_t;

enum {
  ICMP_ECHO_REPLY = 0,
  ICMP_ECHO_REQ = 8,
};

int icmp_echo_reply(nic_t *nic, ipv4_t source,
                    icmp_packet_t *packet,
                    size_t size)
{
  int error = 0;
  icmp_packet_t *reply = ipv4_packet_new
    (ETH_FRAME_STATIC, nic, IP_PROTO_ICMP,
     size, source, &error);

  reply->type = ICMP_ECHO_REPLY;
  reply->code = 0;
  reply->checksum = packet->checksum - reply->type + packet->type;
  memcpy(reply->payload, packet->payload, size);

  return ipv4_transmit(nic, reply, size);
}

int icmp_receive_packet(nic_t *nic, ipv4_t source, void *_packet, size_t size)
{
#if DEBUG_LOCAL
  serial_printf("[icmp] packet size: %u\n", size);
#endif

  if (size < sizeof(icmp_packet_t)) return -1;

  icmp_packet_t *packet = _packet;

  switch (packet->type) {
  case ICMP_ECHO_REQ:
    icmp_echo_reply(nic, source, packet, size);
  }

  return 0;
}

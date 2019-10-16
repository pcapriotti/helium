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
  uint8_t sender_mac[6];
  uint32_t sender_ip;
  uint8_t target_mac[6];
  uint32_t target_ip;
} __attribute__((packed)) arp_packet_t;

enum {
  OP_REQUEST = 1,
  OP_REPLY = 2,
};

typedef struct arp_data {
  arp_packet_t buffer[ARP_BUFSIZE];
  unsigned int begin, end;
  semaphore_t buffer_sem;
  semaphore_t buffer_ready;
} arp_data_t;

arp_data_t arp_data = {0};

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

  /* copy packet into arp buffer */
  sem_wait(&arp_data.buffer_sem);
  memcpy(&arp_data.buffer[arp_data.end], packet, sizeof(arp_packet_t));
  arp_data.end = (arp_data.end + 1) % ARP_BUFSIZE;
  sem_signal(&arp_data.buffer_sem);

  /* signal the processing thread */
  sem_signal(&arp_data.buffer_ready);
}

void process_packets(void)
{
  arp_data_t *data = &arp_data;

  while (1) {
    /* wait until there are new packets to process */
    sem_wait(&data->buffer_ready);

    /* process all packets */
    while (1) {
      sem_wait(&data->buffer_sem);
      if (data->begin == data->end) {
        sem_signal(&data->buffer_sem);
        break;
      }

      arp_packet_t *packet = &data->buffer[data->begin];
      data->begin = (data->begin + 1) % ARP_BUFSIZE;

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

      sem_signal(&data->buffer_sem);
    }
  }
}

void arp_init(void)
{
  arp_data_t *data = &arp_data;

  sem_init(&data->buffer_sem, 1);
  sem_init(&data->buffer_ready, 0);

  sched_spawn_task(process_packets);
}


#define HT_KEY_TYPE uint32_t
#define HT_NAME u32

#include "hashtable.h"
#include "heap.h"
#include "network/udp.h"
#include "network/network.h"
#include "network/types.h"

#include <arpa/inet.h>

#define DEBUG_LOCAL 1

typedef struct udp_header {
  uint16_t src_port;
  uint16_t dst_port;
  uint16_t length;
  uint16_t checksum;
} __attribute__((packed)) udp_header_t;

uint16_t udp_header_src_port(udp_header_t *header)
{
  return ntohs(header->src_port);
}

uint16_t udp_header_dst_port(udp_header_t *header)
{
  return ntohs(header->dst_port);
}

uint16_t udp_header_length(udp_header_t *header)
{
  return ntohs(header->length);
}

typedef struct udp_handler {
  void *data;
  udp_on_packet_t handle;
} udp_handler_t;

static hashtable_u32_t *udp_handlers = 0;

hashtable_u32_t *udp_get_handlers()
{
  if (!udp_handlers) {
    udp_handlers = ht_u32_new(network_get_heap());
  }
  return udp_handlers;
}

int udp_grab_port(uint16_t port,
                  udp_on_packet_t on_packet,
                  void *on_packet_data)
{
  heap_t *heap = network_get_heap();
  hashtable_u32_t *handlers = udp_get_handlers();
  udp_handler_t *handler = ht_u32_get(handlers, port);
  if (handler) {
#if DEBUG_LOCAL
    int col = serial_set_colour(SERIAL_COLOUR_WARN);
    serial_printf("[udp] port %u has already been grabbed\n", port);
    serial_set_colour(col);
#endif
    return -1;
  }

  handler = heap_malloc(heap, sizeof(udp_handler_t));
  handler->data = on_packet_data;
  handler->handle = on_packet;
  ht_u32_insert(udp_get_handlers(), port, handler);
  return 0;
}

int udp_receive_packet(nic_t *nic, ipv4_t source,
                       void *packet, size_t size)
{
  if (size < sizeof(udp_header_t)) {
#if DEBUG_LOCAL
    int col = serial_set_colour(SERIAL_COLOUR_WARN);
    serial_printf("[udp] packet is too small\n");
    serial_set_colour(col);
#endif
    return -1;
  }
  udp_header_t *header = packet;

#if DEBUG_LOCAL
  serial_printf("[udp] receving packet, port: %u, size: %u, length: %u\n",
                udp_header_dst_port(header),
                size,
                udp_header_length(header));
#endif

  udp_handler_t *handler = ht_u32_get(udp_get_handlers(),
                                      udp_header_dst_port(header));
  if (!handler) {
#if DEBUG_LOCAL
    int col = serial_set_colour(0);
    serial_printf("  unhandled packed\n");
    serial_set_colour(col);
#endif
    return -1;
  }

  handler->handle(handler->data, nic,
                  source, udp_header_src_port(header),
                  packet + sizeof(udp_header_t),
                  size - sizeof(udp_header_t));

  return 0;
}

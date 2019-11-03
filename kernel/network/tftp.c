#include "core/debug.h"
#include "core/serial.h"
#include "network/network.h"
#include "network/tftp.h"
#include "network/types.h"
#include "network/udp.h"

#define DEBUG_LOCAL 1

typedef struct tftp_header {
  uint8_t _unused;
  uint8_t opcode;
} __attribute__((packed)) tftp_header_t;

typedef struct tftp_req {
  const char *filename;
  const char *mode;
} tftp_req_t;

typedef struct tftp_ack_packet {
  tftp_header_t header;
  uint16_t block;
} __attribute__((packed)) tftp_ack_packet_t;

enum {
  OPCODE_RRQ = 1,
  OPCODE_WRQ,
  OPCODE_DATA,
  OPCODE_ACK,
  OPCODE_ERR,
};

static uint16_t tftp_port = 0;

static int tftp_parse_req(tftp_req_t *req, uint8_t *payload, size_t length)
{
  /* make sure the payload ends with a terminator */
  if (payload[length - 1]) {
#if DEBUG_LOCAL
    int col = serial_set_colour(SERIAL_COLOUR_WARN);
    serial_printf("[tftp] packet is not terminated correctly\n");
    serial_set_colour(col);
#endif
    payload[length - 1] = '\0';
  }

  /* find filename terminator */
  for (size_t i = 0; i < length - 1; i++) {
    if (!payload[i]) {
      req->filename = (const char *) payload;
      req->mode = (const char *) &payload[i + 1];
      return 0;
    }
  }

  return -1;
}

static void tftp_ack(nic_t *nic,
                     ipv4_t ip, uint16_t port,
                     uint16_t block)
{
  int error = 0;
  tftp_ack_packet_t *ack = udp_packet_new
    (ETH_FRAME_STATIC, nic, tftp_port, ip, port,
     sizeof(tftp_ack_packet_t), &error);
  if (!ack) {
#if DEBUG_LOCAL
    int col = serial_set_colour(SERIAL_COLOUR_ERR);
    serial_printf("[tftp] could not allocate packet, error = %d\n", error);
    serial_set_colour(col);
#endif
    return;
  }
  ack->header.opcode = OPCODE_ACK;
  ack->block = block;
  udp_transmit(nic, ack, sizeof(tftp_ack_packet_t));
}

static void tftp_receive_packet(void *data, nic_t *nic,
                                ipv4_t src, uint16_t src_port,
                                void *payload, size_t size)
{
  if (size < sizeof(tftp_header_t)) {
#if DEBUG_LOCAL
    int col = serial_set_colour(SERIAL_COLOUR_WARN);
    serial_printf("[tftp] packet is too small\n");
    serial_set_colour(col);
#endif
    return;
  }

  tftp_header_t *header = payload;
  payload += sizeof(tftp_header_t);
  size -= sizeof(tftp_header_t);

#ifdef DEBUG_LOCAL
  serial_printf("[tftp] received packet, size: %u, opcode: %u\n",
                size, header->opcode);
#endif

  switch (header->opcode) {
  case OPCODE_RRQ:
  case OPCODE_WRQ:
    {
      tftp_req_t req;
      int ret = tftp_parse_req(&req, payload, size);
      if (ret != -1) {
#ifdef DEBUG_LOCAL
        serial_printf("[tftp] filename: %s, mode: %s\n",
                      req.filename, req.mode);
#endif
        /* send ack */
        tftp_ack(nic, src, src_port, 0);
      }
    }
    break;
  }
}

void tftp_start_server(uint16_t port)
{
  tftp_port = port;
  udp_grab_port(port, tftp_receive_packet, 0);
}

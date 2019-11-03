#include "network/udp.h"
#include "network/network.h"
#include "network/types.h"

#define DEBUG_LOCAL 1

int udp_receive_packet(nic_t *nic, ipv4_t source,
                       void *packet, size_t size)
{
#if DEBUG_LOCAL
  serial_printf("[udp] receving packet\n");
#endif
  return 0;
}

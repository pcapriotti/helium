#include "core/debug.h"
#include "network/icmp.h"

#define DEBUG_LOCAL 1

int icmp_receive_packet(struct nic *nic, void *packet, size_t size)
{
#if DEBUG_LOCAL
  serial_printf("[icmp] packet\n");
#endif

  return 0;
}

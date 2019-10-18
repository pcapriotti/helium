#include "atomic.h"
#include "core/debug.h"
#include "drivers/drivers.h"
#include "drivers/rtl8139/driver.h"
#include "core/gdt.h"
#include "core/interrupts.h"
#include "core/io.h"
#include "handlers.h"
#include "memory.h"
#include "network/network.h"
#include "pci.h"
#include "scheduler.h"
#include "semaphore.h"

#define DEBUG_LOCAL 1

#define RXBUF_SIZE 8192
#define NUM_TX_SLOTS 4

typedef struct {
  uint16_t info;
  uint16_t length;
  uint8_t payload[];
} __attribute__((packed)) packet_t;

typedef struct {
  uint8_t irq;
  uint16_t iobase;
  mac_t mac;

  uint8_t *rx;

  uint8_t tx_index;
  semaphore_t tx_index_mutex;

  semaphore_t tx_sem;

  nic_on_packet_t on_packet;
  void *on_packet_data;
  semaphore_t on_packet_sem;

  uint8_t rxbuf[RXBUF_SIZE + 0x10];
} data_t;

data_t rtl8139_data = {0};

/* rx tasklet */
static uint32_t tasklet_stack[256];
static task_t tasklet = {
  .state = TASK_STOPPED,
};
static int tasklet_running = 0;

enum {
  REG_MAC = 0x00,
  REG_TSD = 0x10,
  REG_TSAD = 0x20,
  REG_RBSTART = 0x30,
  REG_CMD = 0x37,
  REG_CAPR = 0x38,
  REG_CBR = 0x3a,
  REG_INT_MASK = 0x3c,
  REG_INT_STATUS = 0x3e,
  REG_RX_CONF = 0x44,
  REG_CONFIG_1 = 0x52,
};

enum {
  TSD_TOK = 1 << 15,
  TSD_TUN = 1 << 14,
  TSD_OWN = 1 << 13,
};

enum {
  CMD_BUFE = 1 << 0,
  CMD_TE = 1 << 2,
  CMD_RE = 1 << 3,
  CMD_RST = 1 << 4,
};

enum {
  INT_MASK_ROK = 1 << 0,
  INT_MASK_RER = 1 << 1,
  INT_MASK_TOK = 1 << 2,
  INT_MASK_TER = 1 << 3,
  INT_MASK_RXOVW = 1 << 4,
  INT_MASK_PUN = 1 << 5,
  INT_MASK_FOVW = 1 << 6,
  INT_MASK_LEN_CHG = 1 << 13,
  INT_MASK_TIMEOUT = 1 << 14,
  INT_MASK_SERR = 1 << 15,
};

enum {
  RX_CONF_AAP = 1 << 0, /* all packets */
  RX_CONF_APM = 1 << 1, /* physical match */
  RX_CONF_AM = 1 << 2, /* multicast */
  RX_CONF_AB = 1 << 3, /* broadcast */
  RX_CONF_WRAP = 1 << 7,
};

uint16_t iobase(device_t *dev)
{
  for (int i = 0; i < PCI_NUM_BARS; i++) {
    if (dev->bars[i] && dev->bars[i] & 1) {
      return dev->bars[i] & ~3;
    }
  }

  return 0;
}

int transmit(void *_data, void *buf, size_t len)
{
  data_t *data = _data;

  assert(len <= 0x700); /* maximum tx packet size */

#if DEBUG_LOCAL
  serial_printf("[rtl8139] tx (len %u): ", len);
  for (size_t i = 0; i < len; i++) {
    serial_printf("%02x ", ((uint8_t *)buf)[i]);
  }
  serial_printf("\n");
#endif

  /* only NUM_TX_SLOTS thread allowed at one time */
  sem_wait(&data->tx_sem);

  /* writing to a tx slot needs to be serialised to make sure that
  tx_index is in sync with the hardware */
  sem_wait(&data->tx_index_mutex);

  const uint8_t index = data->tx_index;
  const uint16_t tsd = data->iobase + REG_TSD + 4 * index;
  const uint16_t tsad = data->iobase + REG_TSAD + 4 * index;
#if DEBUG_LOCAL
  serial_printf("  writing to slot: %d tsd: %#x tsad: %#x\n", index, tsd, tsad);
#endif
  outl(tsad, (size_t) buf);
  outl(tsd, len & 0x1fff);

  /* switch to the next slot */
  data->tx_index = (data->tx_index + 1) % NUM_TX_SLOTS;
  sem_signal(&data->tx_index_mutex);

  /* tx_sem will be signalled when the card notifies that the
  transmission is successful */
  return 0;
}

void receive(void)
{
  data_t *data = &rtl8139_data;

  while (1) {
#if DEBUG_LOCAL
    serial_printf("[rtl8139] rx capr: %#x: cbr: %#x\n",
                  inw(data->iobase + REG_CAPR),
                  inw(data->iobase + REG_CBR));
#endif
    while (!(inb(data->iobase + REG_CMD) & CMD_BUFE)) {
      packet_t *packet = (packet_t *)data->rx;

      uint16_t cbr = inw(data->iobase + REG_CBR);
      uint16_t capr = inw(data->iobase + REG_CAPR);

#if DEBUG_LOCAL
      serial_printf("[rtl8139] rx info: %#x length: %#x\n",
                    packet->info, packet->length);
      for (int i = 0; i < packet->length; i++) {
        serial_printf("%02x ", packet->payload[i]);
      }
      serial_printf("\n");
#endif

      if (data->on_packet) {
        data->on_packet(data->on_packet_data, &rtl8139_nic, packet->payload, packet->length);
      }

      /* advance rx pointer and align */
      data->rx += packet->length + 4;
      data->rx = (uint8_t *) ALIGNED(data->rx + 3, 2);
      while (data->rx > data->rxbuf + RXBUF_SIZE) {
        data->rx -= RXBUF_SIZE;
      }

      uint16_t offset = data->rx - data->rxbuf - 0x10;
      outw(data->iobase + REG_CAPR, offset);
    }

    uint16_t intr = inw(data->iobase + REG_INT_STATUS);
    outw(data->iobase + REG_INT_STATUS, intr | INT_MASK_ROK);

    sched_disable_preemption();
    sched_current->state = TASK_WAITING;
    tasklet_running = 0;
    serial_printf("[rtl8139] eoi\n");
    pic_eoi(data->irq);
    sched_yield();
  }
}

int rtl8139_matches(void *data, device_t *dev)
{
  return dev->id == 0x813910ec;
}

mac_t rtl8139_get_mac(void *_data)
{
  data_t *data = _data;

  return data->mac;
}

int rtl8139_grab(void *_data,
                 nic_on_packet_t on_packet,
                 void *on_packet_data)
{
  data_t *data = _data;

  sem_wait(&data->on_packet_sem);
  if (on_packet && data->on_packet) {
    serial_printf("rtl8139 has already been grabbed\n");
    return -1;
  }
  data->on_packet = on_packet;
  data->on_packet_data = on_packet_data;
  sem_signal(&data->on_packet_sem);
  return 0;
}

void rtl8139_irq(isr_stack_t *stack)
{
  data_t *data = &rtl8139_data;

  uint16_t intr = inw(data->iobase + REG_INT_STATUS);
  outw(data->iobase + REG_INT_STATUS, intr);
#ifdef DEBUG_LOCAL
  serial_printf("[rtl8139] interrupt: %#x flags: %#x\n", intr, cpu_flags());
#endif
  if (intr & INT_MASK_ROK) {
    if (!tasklet_running) {
      tasklet.state = TASK_RUNNING;
      list_add(&sched_runqueue, &tasklet.head);
      tasklet_running = 1;
    }
  }
  if (intr & (INT_MASK_TOK | INT_MASK_TER)) {
    _sem_signal(&data->tx_sem);
  }

  pic_eoi(data->irq);
}

int rtl8139_init(void *_data, device_t *dev)
{
  data_t *data = _data;

  data->iobase = iobase(dev);
  data->irq = dev->irq;
  data->rx = data->rxbuf;
  sem_init(&data->tx_index_mutex, 1);
  sem_init(&data->tx_sem, NUM_TX_SLOTS);
  sem_init(&data->on_packet_sem, 1);

#if DEBUG_LOCAL
  serial_printf("[rtl8139] irq: %u\n", data->irq);
  serial_printf("[rtl8139] iobase: %#x\n", data->iobase);
#endif

  if (!data->iobase) {
#if DEBUG_LOCAL
    serial_printf("[rtl8139] FATAL: could not find IO base address for device\n");
    return -1;
#endif
  }

  /* initialise rx tasklet */
  isr_stack_t *stack = (void *)tasklet_stack + sizeof(tasklet_stack) - sizeof(isr_stack_t);
  stack->eip = (uint32_t) receive;
  stack->eflags = EFLAGS_IF;
  stack->cs = GDT_SEL(GDT_CODE);
  tasklet.stack = stack;
  tasklet.state = TASK_WAITING;

  /* set bus master bit in PCI configuration */
  device_command_set_mask(dev, PCI_CMD_BUS_MASTER);

  /* reset */
  outb(data->iobase + REG_CONFIG_1, 0);
  outb(data->iobase + REG_CMD, CMD_RST);
  while (inb(data->iobase + REG_CMD) & CMD_RST);

  /* set receive buffer */
  outl(data->iobase + REG_RBSTART, (size_t) data->rxbuf);

  /* set interrupt mask */
  outw(data->iobase + REG_INT_MASK, INT_MASK_ROK | INT_MASK_TOK);
       /* INT_MASK_TOK | INT_MASK_ROK | */
       /* INT_MASK_TER | INT_MASK_RER); */

  /* configure rx */
  outl(data->iobase + REG_RX_CONF,
       RX_CONF_AAP | RX_CONF_APM |
       RX_CONF_AM | RX_CONF_AB |
       RX_CONF_WRAP);

  /* start rx and tx */
  outb(data->iobase + REG_CMD, CMD_TE | CMD_RE);

  /* store mac address */
  for (int i = 0; i < 6; i++) {
    data->mac.data[i] = inb(data->iobase + REG_MAC + i);
  }

#if DEBUG_LOCAL
  serial_printf("[rtl8139] mac: ");
  debug_mac(data->mac);
  serial_printf("\n");
#endif

  /* register irq */
  int ret = irq_grab(data->irq, rtl8139_irq);
  if (ret == -1) {
#if DEBUG_LOCAL
    serial_printf("[rtl8139] could not register irq handler\n");
#endif
  }

  return 0;
}

driver_t rtl8139_driver = {
  .matches = rtl8139_matches,
  .init = rtl8139_init,
  .data = &rtl8139_data,
};

nic_ops_t rtl8139_ops = {
  .grab = rtl8139_grab,
  .transmit = transmit,
  .mac = rtl8139_get_mac,
};

nic_t rtl8139_nic = {
  .name = "eth0",
  .ops = &rtl8139_ops,
  .ops_data = &rtl8139_data,
  .ip = 0x0205a8c0,
};

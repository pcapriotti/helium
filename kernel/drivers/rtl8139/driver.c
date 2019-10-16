#include "core/debug.h"
#include "drivers/drivers.h"
#include "drivers/rtl8139/driver.h"
#include "core/gdt.h"
#include "core/interrupts.h"
#include "core/io.h"
#include "memory.h"
#include "pci.h"
#include "scheduler.h"

#define DEBUG_LOCAL 1

#define RXBUF_SIZE 8192

typedef uint8_t mac_t[6];

void debug_mac(mac_t mac)
{
  for (int i = 0; i < 6; i++) {
    serial_printf("%s%02x", i == 0 ? "" : ":", mac[i]);
  }
}

typedef struct eth_frame {
  mac_t destination;
  mac_t source;
  uint16_t type;
  uint8_t payload[];
} __attribute__((packed)) eth_frame_t;

typedef struct {
  uint16_t info;
  uint16_t length;
  eth_frame_t frame;
} __attribute__((packed)) packet_t;

typedef struct {
  uint8_t irq;
  uint16_t iobase;

  uint8_t rxbuf[RXBUF_SIZE + 0x10];
  uint8_t *rx;
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
  CMD_BUFE = (1 << 0),
  CMD_TE = (1 << 2),
  CMD_RE = (1 << 3),
  CMD_RST = (1 << 4),
};

enum {
  INT_MASK_ROK = (1 << 0),
  INT_MASK_RER = (1 << 1),
  INT_MASK_TOK = (1 << 2),
  INT_MASK_TER = (1 << 3),
  INT_MASK_RXOVW = (1 << 4),
  INT_MASK_PUN = (1 << 5),
  INT_MASK_FOVW = (1 << 6),
  INT_MASK_LEN_CHG = (1 << 13),
  INT_MASK_TIMEOUT = (1 << 14),
  INT_MASK_SERR = (1 << 15),
};

enum {
  RX_CONF_AAP = (1 << 0), /* all packets */
  RX_CONF_APM = (1 << 1), /* physical match */
  RX_CONF_AM = (1 << 2), /* multicast */
  RX_CONF_AB = (1 << 3), /* broadcast */
  RX_CONF_WRAP = (1 << 7),
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

void handle_rx(void)
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
        serial_printf("%02x ", data->rx[i]);
      }
      serial_printf("\n");
      debug_mac(packet->frame.source);
      serial_printf(" => ");
      debug_mac(packet->frame.destination);
      serial_printf("\n");
#endif

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
    pic_eoi(data->irq);
    sched_yield();
  }
}

int rtl8139_init(void *_data, device_t *dev)
{
  data_t *data = _data;

  data->iobase = iobase(dev);
  data->irq = dev->irq;
  data->rx = data->rxbuf;

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
  stack->eip = (uint32_t) handle_rx;
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
  outw(data->iobase + REG_INT_MASK,
       INT_MASK_TOK | INT_MASK_ROK |
       INT_MASK_TER | INT_MASK_RER |
       INT_MASK_TIMEOUT | INT_MASK_SERR);

  /* configure rx */
  outl(data->iobase + REG_RX_CONF,
       RX_CONF_AAP | RX_CONF_APM | RX_CONF_AM | RX_CONF_AB);

  /* start rx and tx */
  outb(data->iobase + REG_CMD, CMD_TE | CMD_RE);

#if DEBUG_LOCAL
  serial_printf("[rtl8139] cmd: %#02x\n", inb(data->iobase + REG_CMD));
  serial_printf("[rtl8139] mac: ");
  for (int i = 0; i < 6; i++) {
    serial_printf("%s%02x", i == 0 ? "" : ":",
                  inb(data->iobase + REG_MAC + i));
  }
  serial_printf("\n");
#endif

  return 0;
}

int rtl8139_matches(void *data, device_t *dev)
{
  return dev->id == 0x813910ec;
}

void rtl8139_irq(void)
{
  data_t *data = &rtl8139_data;

  uint16_t intr = inw(data->iobase + REG_INT_STATUS);
  if (intr & INT_MASK_ROK) {
    if (!tasklet_running) {
      tasklet.state = TASK_RUNNING;
      list_add(&sched_runqueue, &tasklet.head);
      tasklet_running = 1;
    }
  }
}

driver_t rtl8139_driver = {
  .matches = rtl8139_matches,
  .init = rtl8139_init,
  .data = &rtl8139_data,
};
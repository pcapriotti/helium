#include "core/gdt.h"
#include "core/interrupts.h"
#include "core/io.h"
#include "core/serial.h"
#include "drivers/drivers.h"
#include "drivers/realtek/common.h"
#include "drivers/realtek/rtl8169.h"
#include "handlers.h"
#include "memory.h"
#include "network/types.h"
#include "network/network.h"
#include "pci.h"
#include "scheduler.h"

#include <stdint.h>
#include <string.h>

#define DEBUG_LOCAL 1

#define MAX_RX_DESC 1024
#define MAX_TX_DESC 1024
#define RX_BUFSIZE ETH_MTU
#define TX_BUFSIZE ETH_MTU

typedef struct descriptor {
  uint32_t flags;
  uint32_t vlan_tag;
  uint64_t buffer;
} __attribute__((packed)) descriptor_t;

void *descriptor_buffer(descriptor_t *desc)
{
  return (void *)(size_t) desc->buffer;
}

uint16_t descriptor_length(descriptor_t *desc)
{
  return desc->flags & 0x3fff;
}

enum {
  DESC_OWN = 1 << 31,
  DESC_EOR = 1 << 30,
  DESC_FS = 1 << 29,
  DESC_LS = 1 << 28,
};

typedef struct rtl8169 {
  uint16_t iobase;
  uint8_t irq;
  mac_t mac;

  /* must be 256 byte aligned */
  descriptor_t *rx_desc;
  int rx_num_desc;
  descriptor_t *tx_desc;
  int tx_num_desc;

  /* receive callback */
  nic_on_packet_t on_packet;
  void *on_packet_data;
} rtl8169_t;

rtl8169_t rtl8169_instance;

enum {
  LOCK_NORMAL = 0 << 6,
  LOCK_CONFIG_WRITE = 3 << 6,
};

static uint32_t tasklet_stack[512];
static task_t tasklet = {
  .state = TASK_STOPPED,
};
static int tasklet_running = 0;

int rtl8169_matches(void *data, device_t *dev)
{
  return dev->id == 0x816810ec;
}

static void rtl8169_receive(void)
{
  rtl8169_t *rtl = &rtl8169_instance;

  while (1) {
    for (int i = 0; i < rtl->rx_num_desc; i++) {
      descriptor_t *desc = &rtl->rx_desc[i];
      if (!(desc->flags & DESC_OWN)) {
        if (!(desc->flags & DESC_LS) ||
            !(desc->flags & DESC_LS)) {
#if DEBUG_LOCAL
          int col = serial_set_colour(SERIAL_COLOUR_WARN);
          serial_printf("[rtl8169] ignoring partial packet\n");
          serial_set_colour(col);
#endif
          continue;
        }
        if (rtl->on_packet) {
          uint8_t *buf = descriptor_buffer(desc);
          rtl->on_packet(rtl->on_packet_data,
                         &rtl8169_nic,
                         buf,
                         descriptor_length(desc));
        }

        desc->flags |= DESC_OWN;
      }
    }

    sched_disable_preemption();
    sched_current->state = TASK_WAITING;
    tasklet_running = 0;
    pic_unmask(rtl->irq);
    sched_yield();
  }
}

void rtl8169_irq(isr_stack_t *stack)
{
  rtl8169_t *rtl = &rtl8169_instance;
  uint16_t status = inw(rtl->iobase + REG_INT_STATUS);
  serial_printf("[rtl8169] irq fired status: %#02x\n", status);
  outw(rtl->iobase + REG_INT_STATUS, status); /* ack */

  if (!tasklet_running) {
    tasklet.state = TASK_RUNNING;
    list_add(&sched_runqueue, &tasklet.head);
    tasklet_running = 1;
  }

  pic_mask(rtl->irq);
  pic_eoi(rtl->irq);
}
HANDLER_STATIC(rtl8169_irq_handler, rtl8169_irq);

static void rtl8169_setup_tx(rtl8169_t *rtl)
{
  /* prepare descriptors */
  for (int i = 0; i < rtl->tx_num_desc; i++) {
    descriptor_t *desc = &rtl->tx_desc[0];
    desc->flags = DESC_OWN;
    desc->vlan_tag = 0;

    void *buf = falloc(TX_BUFSIZE);
    assert(((size_t) buf & 0x7) == 0);
    desc->buffer = (size_t) buf;
  }

  /* set IFG and DMA burst size */
  outl(rtl->iobase + REG_TX_CONF,
       TX_CONF_IFG_NORMAL |
       TX_CONF_DMA_UNLIM);

#if DEBUG_LOCAL
  {
    uint32_t txconf = inl(rtl->iobase + REG_TX_CONF);
    serial_printf("[rtl8169] txconf: %#08x\n", txconf);
  }
#endif

  /* set write threshold */
  outb(rtl->iobase + REG_TX_THRESHOLD, 0x3b);

  /* set descriptor */
  uint64_t descp = (size_t) rtl->tx_desc;
  assert((descp & 0xff) == 0);

  outl(rtl->iobase + REG_TX_DESC_HI, descp >> 32);
  outl(rtl->iobase + REG_TX_DESC_LO, descp);
}

static void rtl8169_setup_rx(rtl8169_t *rtl)
{
  /* prepare descriptors */
  for (int i = 0; i < rtl->rx_num_desc; i++) {
    descriptor_t *desc = &rtl->rx_desc[i];
    desc->flags = DESC_OWN | (RX_BUFSIZE & 0x3fff);
    desc->vlan_tag = 0;

    void *buf = falloc(RX_BUFSIZE);
    assert(((size_t) buf & 0x7) == 0);
    desc->buffer = (size_t) buf;
  }
  rtl->rx_desc[rtl->rx_num_desc - 1].flags |= DESC_EOR;

  /* promiscuous mode */
  uint32_t rxconf =
    RX_CONF_AAP | RX_CONF_APM |
    RX_CONF_AM | RX_CONF_AB |
    RX_CONF_DMA_UNLIM |
    RX_CONF_FIFO_UNLIM;
  outl(rtl->iobase + REG_RX_CONF, rxconf);

  /* set max packet size */
  outw(rtl->iobase + REG_RMS, ETH_MTU);

#if DEBUG_LOCAL
  {
    rxconf = inl(rtl->iobase + REG_RX_CONF);
    serial_printf("[rtl8169] read rxconf: %#08x\n", rxconf);
    uint16_t psize = inl(rtl->iobase + REG_RMS);
    serial_printf("[rtl8169] packet size: %#04x\n", psize);
  }
#endif

  /* set descriptor */
  uint64_t descp = (size_t) rtl->rx_desc;
  assert((descp & 0xff) == 0);
  outl(rtl->iobase + REG_RX_DESC_HI, descp >> 32);
  outl(rtl->iobase + REG_RX_DESC_LO, descp);
}

int rtl8169_init(void *data, device_t *dev)
{
  rtl8169_t *rtl = data;

  rtl->iobase = rtl_find_iobase(dev);
  rtl->irq = dev->irq & 0xff;
  rtl->rx_desc = falloc(MAX_RX_DESC * sizeof(descriptor_t));
  rtl->rx_num_desc = 256;
  rtl->tx_desc = falloc(MAX_TX_DESC * sizeof(descriptor_t));
  rtl->tx_num_desc = 256;
  rtl->on_packet = 0;
  rtl->on_packet_data = 0;

#if DEBUG_LOCAL
  serial_printf("[rtl8169] irq number: %#2x\n", rtl->irq);
#endif

  /* register irq */
  int ret = irq_grab(rtl->irq, &rtl8169_irq_handler);
  if (ret == -1) {
#if DEBUG_LOCAL
    serial_printf("[rtl8169] could not register irq handler\n");
#endif
  }

  if (!rtl->iobase) {
#if DEBUG_LOCAL
    serial_printf("[rtl8169] FATAL: could not find IO base address for device\n");
    return -1;
#endif
  }

  /* prepare tasklet */
  isr_stack_t *stack = (void *)tasklet_stack +
    sizeof(tasklet_stack) - sizeof(isr_stack_t);
  stack->eip = (uint32_t) rtl8169_receive;
  stack->eflags = EFLAGS_IF;
  stack->cs = GDT_SEL(GDT_CODE);
  tasklet.stack = stack;
  tasklet.state = TASK_WAITING;

  /* reset */
  outb(rtl->iobase + REG_CMD, CMD_RST);
  while (inb(rtl->iobase + REG_CMD) & CMD_RST);

  /* unlock config registers */
  outb(rtl->iobase + REG_LOCK, LOCK_CONFIG_WRITE);
  outw(rtl->iobase + REG_CMD_PLUS,
       inw(rtl->iobase + REG_CMD_PLUS));

  rtl8169_setup_rx(rtl);
  rtl8169_setup_tx(rtl);

  /* enable rx and tx */
  outb(rtl->iobase + REG_CMD, CMD_TE | CMD_RE);

  /* mask interrupts and ack */
  outw(rtl->iobase + REG_INT_MASK, 0);
  outw(rtl->iobase + REG_INT_STATUS, 0xffff);
#if DEBUG_LOCAL
  {
    uint8_t cmd = inb(rtl->iobase + REG_CMD);
    serial_printf("[rtl8169] cmd: %#02x\n", cmd);
  }
#else
  inb(rtl->iobase + REG_CMD);
#endif

  /* set bus master bit in PCI configuration */
  device_command_set_mask(dev, PCI_CMD_BUS_MASTER);


  /* store mac address */
  for (int i = 0; i < 6; i++) {
    rtl->mac.data[i] = inb(rtl->iobase + REG_MAC + i);
  }

  /* set interrupt mask */
  outw(rtl->iobase + REG_INT_MASK, 0xff);
       /* INT_MASK_ROK | INT_MASK_TOK | */
       /* INT_MASK_RER | INT_MASK_TER); */
  {
    uint16_t status = inw(rtl->iobase + REG_INT_STATUS);
    serial_printf("[rtl8169] int status: %#04x\n", status);
  }

  /* lock config registers */
  outb(rtl->iobase + REG_LOCK, LOCK_NORMAL);

#if DEBUG_LOCAL
  serial_printf("[rtl8169] mac: ");
  debug_mac(rtl->mac);
  serial_printf("\n");
#endif

  return 0;
}

static int rtl8169_grab(void *data,
                        nic_on_packet_t on_packet,
                        void *on_packet_data)
{
  rtl8169_t *rtl = data;
  if (rtl->on_packet) {
#if DEBUG_LOCAL
    int col = serial_set_colour(SERIAL_COLOUR_WARN);
    serial_printf("[rtl8169] device already grabbed\n");
    serial_set_colour(col);
#endif
    return -1;
  }
  rtl->on_packet = on_packet;
  rtl->on_packet_data = on_packet_data;
  return 0;
}

static int rtl8169_transmit(void *data, void *buf, size_t len)
{
#if DEBUG_LOCAL
  serial_printf("[rtl8169] tx:\n");
  {
    int col = serial_set_colour(0);
    uint8_t *payload = buf;
    for (size_t i = 0; i < len; i++) {
      serial_printf("%02x ", payload[i]);
    }
    serial_printf("\n");
    serial_set_colour(col);
  }
#endif
  {
    int col = serial_set_colour(SERIAL_COLOUR_ERR);
    serial_printf("[rtl8169] transmit not implemented\n");
    serial_set_colour(col);
  }
  return 0;
}

mac_t rtl8169_get_mac(void *data)
{
  rtl8169_t *rtl = data;
  return rtl->mac;
}

driver_t rtl8169_driver = {
  .matches = rtl8169_matches,
  .init = rtl8169_init,
  .data = &rtl8169_instance,
};

nic_ops_t rtl8169_ops = {
  .grab = rtl8169_grab,
  .transmit = rtl8169_transmit,
  .mac = rtl8169_get_mac,
};

nic_t rtl8169_nic = {
  .name = "eth0",
  .ops = &rtl8169_ops,
  .ops_data = &rtl8169_instance,
  .ip = 0x9900a8c0,
};

#include "core/gdt.h"
#include "core/interrupts.h"
#include "core/io.h"
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

#define DEBUG_LOCAL 1

#define MAX_RX_DESC 1024
#define MAX_TX_DESC 1024
#define RX_BUFSIZE 4096
#define TX_BUFSIZE 4096

typedef struct descriptor {
  uint32_t flags;
  uint32_t vlan_tag;
  uint64_t buffer;
} __attribute__((packed)) descriptor_t;

enum {
  DESC_OWN = 1 << 31,
  DESC_EOR = 1 << 30,
};

typedef struct rtl8169 {
  uint16_t iobase;
  uint8_t irq;
  mac_t mac;

  /* must be 256 byte aligned */
  descriptor_t *rx_desc;
  descriptor_t *tx_desc;
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
}

void rtl8169_irq(isr_stack_t *stack)
{
  rtl8169_t *rtl = &rtl8169_instance;
  uint16_t status = inw(rtl->iobase + REG_INT_STATUS);
  serial_printf("[rtl8169] irq fired status: %#02x\n", status);
  outw(rtl->iobase + REG_INT_STATUS, status); /* ack */
  pic_eoi(rtl->irq);
}

static void rtl8169_setup_tx(rtl8169_t *rtl)
{
  /* prepare descriptors */
  static const int num_desc = 1;
  void *buf = falloc(TX_BUFSIZE);
  descriptor_t *desc = &rtl->tx_desc[0];
  desc->flags = DESC_OWN;
  desc->vlan_tag = 0;
  desc->buffer = (size_t) buf;

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
  assert((descp & 0xf) == 0);

  outl(rtl->iobase + REG_TX_DESC_HI, descp >> 32);
  outl(rtl->iobase + REG_TX_DESC_LO, descp);
}

static void rtl8169_setup_rx(rtl8169_t *rtl)
{
  /* prepare descriptors */
  static const int num_desc = 256;
  void *buf = falloc(RX_BUFSIZE);

  for (int i = 0; i < num_desc; i++) {
    descriptor_t *desc = &rtl->rx_desc[i];
    desc->flags = DESC_OWN | (RX_BUFSIZE & 0x2fff);
    desc->vlan_tag = 0;
    desc->buffer = (size_t) buf;
  }
  rtl->rx_desc[num_desc - 1].flags |= DESC_EOR;

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
  assert((descp & 0xf) == 0);
  outl(rtl->iobase + REG_RX_DESC_HI, descp >> 32);
  outl(rtl->iobase + REG_RX_DESC_LO, descp);
}

int rtl8169_init(void *data, device_t *dev)
{
  rtl8169_t *rtl = data;

  rtl->iobase = rtl_find_iobase(dev);
  rtl->irq = dev->irq & 0xff;
  rtl->rx_desc = falloc(MAX_RX_DESC * sizeof(descriptor_t));
  rtl->tx_desc = falloc(MAX_TX_DESC * sizeof(descriptor_t));

#if DEBUG_LOCAL
  serial_printf("[rtl8169] irq number: %#2x\n", rtl->irq);
#endif

  /* register irq */
  int ret = irq_grab(rtl->irq, rtl8169_irq);
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

driver_t rtl8169_driver = {
  .matches = rtl8169_matches,
  .init = rtl8169_init,
  .data = &rtl8169_instance,
};

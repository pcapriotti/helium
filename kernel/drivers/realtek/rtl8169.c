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
#define MAX_PACKET_SIZE 0x1fff

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

static void rtl8169_irq(isr_stack_t *stack)
{
  rtl8169_t *rtl = &rtl8169_instance;
  serial_printf("[rtl8169] irq\n");
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

  /* enable tx */
  outb(rtl->iobase + REG_CMD, CMD_TE);

  /* set IFG and DMA burst size */
  outl(rtl->iobase + REG_TX_CONF,
       TX_CONF_IFG_NORMAL |
       TX_CONF_DMA_UNLIM);

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
  static const int num_desc = 1;
  void *buf = falloc(RX_BUFSIZE);
  descriptor_t *desc = &rtl->rx_desc[0];
  desc->flags = DESC_OWN | DESC_EOR |
    (RX_BUFSIZE & 0x1fff);
  desc->vlan_tag = 0;
  desc->buffer = (size_t) buf;

  /* promiscuous mode */
  outl(rtl->iobase + REG_RX_CONF,
       RX_CONF_AAP | RX_CONF_APM |
       RX_CONF_AM | RX_CONF_AB |
       RX_CONF_DMA_UNLIM |
       RX_CONF_FIFO_UNLIM);

  /* set max packet size */
  outw(rtl->iobase + REG_RMS, MAX_PACKET_SIZE);

  /* set descriptor */
  uint64_t descp = (size_t) rtl->rx_desc;
  assert((descp & 0xf) == 0);
  outl(rtl->iobase + REG_RX_DESC_HI, descp >> 32);
  outl(rtl->iobase + REG_RX_DESC_LO, descp);
}

int rtl8169_init(void *data, device_t *dev)
{
  rtl8169_t *rtl = data;
#if DEBUG_LOCAL
  serial_printf("[rtl8169] init\n");
#endif

  rtl->iobase = rtl_find_iobase(dev);
  rtl->irq = dev->irq & 0xff;
  rtl->rx_desc = falloc(MAX_RX_DESC * sizeof(descriptor_t));
  rtl->tx_desc = falloc(MAX_TX_DESC * sizeof(descriptor_t));

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

  uint32_t txconf = inl(rtl->iobase + REG_TX_CONF);
#if DEBUG_LOCAL
  serial_printf("[rtl8169] txconf = %#08x\n", txconf);
#else
  (void) txconf;
#endif

  /* unlock config registers */
  outb(rtl->iobase + REG_LOCK, LOCK_CONFIG_WRITE);
  outw(rtl->iobase + REG_CMD_PLUS, 0);

  rtl8169_setup_rx(rtl);
  rtl8169_setup_tx(rtl);

  /* mask interrupts and ack */
  outw(rtl->iobase + REG_INT_MASK, 0);
  outw(rtl->iobase + REG_INT_STATUS,
       inw(rtl->iobase + REG_INT_STATUS));
#if DEBUG_LOCAL
  {
    uint8_t cmd = inb(rtl->iobase + REG_CMD);
    serial_printf("[rtl8169] cmd = %#02x\n", cmd);
  }
#else
  inb(rtl->iobase + REG_CMD);
#endif

  /* set bus master bit in PCI configuration */
  device_command_set_mask(dev, PCI_CMD_BUS_MASTER);

  /* enable rx and tx */
  outb(rtl->iobase + REG_CMD, CMD_TE | CMD_RE);

  /* store mac address */
  for (int i = 0; i < 6; i++) {
    rtl->mac.data[i] = inb(rtl->iobase + REG_MAC + i);
  }

  /* set interrupt mask */
  outw(rtl->iobase + REG_INT_MASK, 0xff);
       /* INT_MASK_ROK | INT_MASK_TOK | */
       /* INT_MASK_RER | INT_MASK_TER); */

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

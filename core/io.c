#include "io.h"

void pic_eoi(uint8_t irq) {
  if (irq >= 8) {
    /* send to slave too */
    outb(PIC_SLAVE_CMD, PIC_EOI);
  }
  /* always send to master */
  outb(PIC_MASTER_CMD, PIC_EOI);
}

void pic_init(void) {
  /* initialise master at offset 0x20 */
  outb(PIC_MASTER_CMD, 0x11); io_wait();
  outb(PIC_MASTER_DATA, 0x20); io_wait();
  outb(PIC_MASTER_DATA, 0x04); io_wait();
  outb(PIC_MASTER_DATA, 0x01); io_wait();
  outb(PIC_MASTER_DATA, 0x00);

  /* initialise slave at offset 0x28 */
  outb(PIC_SLAVE_CMD, 0x11); io_wait();
  outb(PIC_SLAVE_DATA, 0x28); io_wait();
  outb(PIC_SLAVE_DATA, 0x02); io_wait();
  outb(PIC_SLAVE_DATA, 0x01); io_wait();
  outb(PIC_SLAVE_DATA, 0x00);
}

void pic_mask(uint8_t irq)
{
  uint16_t port;
  if (irq < 8) {
    port = PIC_MASTER_DATA;
  }
  else {
    port = PIC_SLAVE_DATA;
    irq -= 8;
  }
  outb(port, inb(port) | (1 << irq));
}

void pic_unmask(uint8_t irq)
{
  uint16_t port;
  if (irq < 8) {
    port = PIC_MASTER_DATA;
  }
  else {
    port = PIC_SLAVE_DATA;
    irq -= 8;
  }
  outb(port, inb(port) & ~(1 << irq));
}

uint16_t pic_get_mask(void)
{
  return inb(PIC_MASTER_DATA) |
    inb(PIC_SLAVE_DATA) << 8;
}

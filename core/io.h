#ifndef IO_H
#define IO_H

#include <stdint.h>

#define PIC_MASTER_CMD 0x20
#define PIC_MASTER_DATA 0x21
#define PIC_SLAVE_CMD 0xa0
#define PIC_SLAVE_DATA 0xa1
#define PIC_EOI 0x20 /* end of interrupt */

static inline void outb(uint16_t port, uint8_t val) {
  __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
  uint8_t ret;
  __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
  __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
  uint16_t ret;
  __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

static inline void outl(uint16_t port, uint32_t val) {
  __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
  uint32_t ret;
  __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

static inline void io_wait() {
  outb(0x80, 0); /* write to an unused port */
}

void pic_setup();
void pic_eoi(uint8_t irq);
void pic_mask(uint8_t irq);
void pic_unmask(uint8_t irq);

#endif /* IO_H */

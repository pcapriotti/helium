#ifndef STAGE1_H
#define STAGE1_H

#include <stdint.h>

extern uint8_t _loader0_start[];
extern uint8_t _loader0_end[];
extern uint8_t _loader_start[];
extern uint8_t _loader_end[];
extern uint8_t _kernel_start[];
extern uint8_t _bss_start[];
extern uint8_t _bss_end[];

#define GDT_SEL(i) ((uint16_t)((i) * sizeof(gdt_entry_t)))

typedef struct {
  uint16_t limit_low;
  uint16_t base_low;
  uint8_t base_mid;
  uint8_t flags;
  uint8_t granularity;
  uint8_t base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
  uint16_t size;
  void *offset;
} __attribute__((packed)) gdtp_t;

#endif /* STAGE1_H */

#ifndef GDT_H
#define GDT_H

#include "stdint.h"

extern uint16_t *vga_text;

void text_panic();

typedef struct {
  uint16_t offset;
  uint16_t segment;
} __attribute__((packed)) ptr16_t;

static inline uint32_t seg_off_to_linear(uint16_t seg, uint16_t off)
{
  return (seg << 4) + off;
}

static inline uint32_t ptr16_to_linear(ptr16_t ptr)
{
  return seg_off_to_linear(ptr.segment, ptr.offset);
}

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

typedef struct {
  uint16_t offset_low;
  uint16_t segment;
  uint16_t flags;
  uint16_t offset_high;
} __attribute__((packed)) idt_entry_t;

typedef struct {
  uint32_t prev;
  uint32_t esp0, ss0;
  uint32_t esp1, ss1;
  uint32_t esp2, ss2;
  uint32_t cr3, eip, eflags;
  uint32_t eax, ecx, edx, ebx;
  uint32_t esp, ebp, esi, edi;
  uint32_t es, cs, ss, ds, fs, gs;
  uint32_t ldt;
  uint16_t trap;
  uint16_t iomap_base;
} __attribute__((packed)) tss_t;

enum {
  EFLAGS_CF = 1 << 0,
  EFLAGS_IF = 1 << 9,
  EFLAGS_VM = 1 << 17,
};

#define V8086_STACK_BASE 0x2000
#define V8086_HEAP 0x2800

typedef struct {
  uint16_t ax, bx, cx, dx, di, bp;
  uint16_t es, ds, fs, gs;
} regs16_t;

int v8086_enter(uint32_t interrupt, regs16_t *regs);
void v8086_exit(void *stack);

#endif /* GDT_H */

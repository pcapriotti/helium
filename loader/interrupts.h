#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include "stage1.h"

enum {
  IDT_GP = 0xd,
  IDT_IRQ = 0x20,
  IDT_NUM_ENTRIES = 0x100,
};

enum {
  NUM_ISR = 0x14,
  NUM_IRQ = 0x10,
};

extern idt_entry_t kernel_idt[IDT_NUM_ENTRIES];

typedef struct {
  tss_t tss;
  uint8_t iomap[8192];
} __attribute__((packed)) tss_with_iomap_t;

extern tss_with_iomap_t kernel_tss;

typedef struct {
  uint32_t edi, esi, ebp, esp_, ebx, edx, ecx, eax;
  uint32_t int_num, error;
  uint32_t eip, cs, eflags, esp, ss;

  /* these are only present when returning from v8086 */
  uint32_t es, ds, fs, gs;
} __attribute__((packed)) isr_stack_t;

typedef struct {
  uint8_t code[10];
} isr_t;

extern isr_t kernel_isr[NUM_ISR + NUM_IRQ];
void isr_generic();

#endif /* INTERRUPTS_H */

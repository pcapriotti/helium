#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>

enum {
  IDT_GP = 0xd,
  IDT_IRQ = 0x20,
  IDT_SYSCALL = 0x7f,
  IDT_NUM_ENTRIES = 0x100,
};

enum {
  NUM_ISR = 0x14,
  NUM_IRQ = 0x10,
};

enum {
  IRQ_TIMER = 0,
  IRQ_KEYBOARD = 1,
};

#define IRQ_MASK(irq) (1 << (irq))

typedef struct {
  uint16_t offset_low;
  uint16_t segment;
  uint16_t flags;
  uint16_t offset_high;
} __attribute__((packed)) idt_entry_t;

void set_idt_entry(idt_entry_t *entry,
                   uint32_t offset,
                   uint16_t segment,
                   uint8_t size32,
                   uint8_t dpl);

extern idt_entry_t kernel_idt[IDT_NUM_ENTRIES];

typedef struct {
  uint16_t size;
  void *offset;
} __attribute__((packed)) idtp_t;

extern idtp_t kernel_idtp;

void set_kernel_idt();

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

typedef struct {
  tss_t tss;
  uint8_t iomap[8192];
} __attribute__((packed)) tss_with_iomap_t;

extern tss_with_iomap_t kernel_tss;

typedef struct isr_stack {
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
void isr_assemble(isr_t *isr, uint8_t number);

/* syscalls */

enum {
  SYSCALL_YIELD,
};

static inline void syscall_yield(void)
{
  __asm__ volatile
    ("int %0\n"
     :
     : "i"(IDT_SYSCALL), "a"(SYSCALL_YIELD));
}

#endif /* INTERRUPTS_H */

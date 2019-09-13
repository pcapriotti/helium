#ifndef INTERRUPTS_H
#define INTERRUPTS_H

enum {
  IDT_GP = 13,
  IDT_PF = 14,
  IDT_NUM_ENTRIES = 0x100,
};

enum {
  NUM_ISR = 0x14,
  IRQ_OFFSET = 0x20,
  NUM_IRQ = 0x10,
};

extern idt_entry_t kernel_idt[IDT_NUM_ENTRIES];

typedef struct {
  tss_t tss;
  uint8_t iomap[8192];
} __attribute__((packed)) tss_with_iomap_t;

extern tss_with_iomap_t kernel_tss;


#endif /* INTERRUPTS_H */

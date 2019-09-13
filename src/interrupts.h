#ifndef INTERRUPTS_H
#define INTERRUPTS_H

enum {
  IDT_NUM_ENTRIES = 0x100
};

extern idt_entry_t kernel_idt[IDT_NUM_ENTRIES];

#endif /* INTERRUPTS_H */

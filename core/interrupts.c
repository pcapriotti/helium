#include "interrupts.h"

__attribute__((aligned(8))) idt_entry_t kernel_idt[IDT_NUM_ENTRIES];

tss_with_iomap_t kernel_tss;

isr_t kernel_isr[NUM_ISR + NUM_IRQ];

#include "gdt.h"
#include "interrupts.h"

__attribute__((aligned(8))) idt_entry_t kernel_idt[IDT_NUM_ENTRIES];

#include "gdt.h"
#include "interrupts.h"

__attribute__((aligned(8))) idt_entry_t kernel_idt[IDT_NUM_ENTRIES];

tss_with_iomap_t kernel_tss;

isr_t kernel_isr[NUM_ISR + NUM_IRQ];

void set_idt_entry(idt_entry_t *entry,
                   uint32_t offset,
                   uint16_t segment,
                   uint8_t size32,
                   uint8_t dpl)
{
  entry->offset_low = offset & 0xffff;
  entry->offset_high = (offset >> 16) & 0xffff;
  entry->segment = segment;
  entry->flags = 0x8600 |
    ((size32 != 0) << 11) |
    (dpl << 13);
}

/* Create code for an isr stub */
void isr_assemble(isr_t *isr, uint8_t number)
{
  int push = 1;
  if (number == 8 || number == 10 || number == 11 || number == 12 ||
      number == 13 || number == 14 || number == 17) {
    push = 0;
  }

  uint8_t *p = isr->code;
  if (push) *p++ = 0x50; /* push ax */

  *p++ = 0x6a; /* push */
  *p++ = number;

  int32_t rel = (int32_t)isr_generic - (int32_t)(p + 5);
  *p++ = 0xe9; /* jump */
  *((int32_t *) p) = rel;
}

void set_kernel_idt()
{
  /* set all entries to 0 */
  for (int i = 0; i < IDT_NUM_ENTRIES; i++) {
    idt_entry_t *entry = &kernel_idt[i];
    entry->offset_low = 0;
    entry->offset_high = 0;
    entry->segment = 0;
    entry->flags = 0;
  }

  /* isr */
  for (int i = 0; i < NUM_ISR; i++) {
    isr_assemble(&kernel_isr[i], i);
    set_idt_entry(&kernel_idt[i],
                  (uint32_t)&kernel_isr[i],
                  GDT_CODE * sizeof(gdt_entry_t),
                  1, 0);
  }

  /* irq */
  for (int i = 0; i < NUM_IRQ; i++) {
    isr_assemble(&kernel_isr[NUM_ISR + i], IDT_IRQ + i);
    set_idt_entry(&kernel_idt[i + IDT_IRQ],
                  (uint32_t)&kernel_isr[i + NUM_ISR],
                  GDT_CODE * sizeof(gdt_entry_t),
                  1, 0);
  }

  __asm__ volatile("lidt (%0)" : : "m"(kernel_idtp));
}

idtp_t kernel_idtp = {
  sizeof(kernel_idt) - 1,
  kernel_idt,
};

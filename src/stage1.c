#include "gdt.h"
#include "handlers.h"
#include "interrupts.h"
#include "stdint.h"
#include "v8086.h"

extern volatile uint8_t *vesa_framebuffer;
extern unsigned short vesa_pitch;

typedef struct {
  uint32_t prev;
  uint32_t esp0, ss0;
  uint32_t esp1, ss1;
  uint32_t esp2, ss2;
  uint32_t cr3, eip, eflags;
  uint32_t eax, ecx, edx, ebx;
  uint32_t esp, ebp, esi, edi;
  uint32_t es, cs, ss, ds, fs, gs;
  uint32_t ldt, trap, iomap_base;
} __attribute__((packed)) tss_t;

static tss_t kernel_tss;

/* GDT */

enum {
  GDT_NULL = 0,
  GDT_CODE,
  GDT_DATA,
  GDT_TASK,

  GDT_NUM_ENTRIES,
};

static __attribute__((aligned(8))) gdt_entry_t kernel_gdt[] = {
  { 0, 0, 0, 0, 0, 0 },
  { 0xffff, 0, 0, 0x9a, 0xcf, 0 }, /* code segment */
  { 0xffff, 0, 0, 0x92, 0xcf, 0 }, /* data segment */
  { 0, 0, 0, 0, 0, 0 }, /* placeholder for task descriptor */
};

gdtp_t kernel_gdtp = {
  sizeof(kernel_gdt) - 1,
  kernel_gdt
};

void set_gdt_entry(gdt_entry_t *entry,
                   uint32_t base,
                   uint32_t limit,
                   uint8_t flags,
                   uint8_t granularity)
{
  entry->limit_low = limit & 0xffff;
  entry->granularity = ((granularity << 4) & 0xf0) |
    ((limit >> 16) & 0x0f);
  entry->base_low = base & 0xffff;
  entry->base_mid = (base >> 16) & 0xff;
  entry->base_high = (base >> 24) & 0xff;
  entry->flags = flags;
}

/* IDT */

gdtp_t kernel_idtp = {
  sizeof(kernel_idt) - 1,
  kernel_idt,
};

typedef struct {
  uint32_t gs, fs, es, ds;
  uint32_t edi, esi, ebp, esp_, ebx, edx, ecx, eax;
  uint32_t int_num, error;
  uint32_t eip, cs, eflags, esp, ss;
} __attribute__((packed)) isr_stack_t;

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

void irq_handler(isr_stack_t *stack)
{
}

void show_error_code(int colour)
{
  for (int j = 0; j < 10; j++) {
    for (int i = 0; i < 10; i++) {
      vesa_framebuffer[i + j * vesa_pitch] = colour;
    }
  }

  __asm__ volatile("hlt");
  while (1);
}

void interrupt_handler(isr_stack_t stack)
{
  show_error_code(1);
}

void _stage1()
{
  set_gdt_entry(&kernel_gdt[GDT_TASK],
                (uint32_t)&kernel_tss,
                sizeof(kernel_tss),
                0x89, 0);
  kernel_tss.ss0 = GDT_SEL(GDT_DATA);
  __asm__ volatile("ltr %0" : : "r"(GDT_SEL(GDT_TASK)));

  for (int i = 0; i < 20; i++) {
    uint32_t size = (uint8_t *)isr1 - (uint8_t *)isr0;
    uint32_t addr = (uint32_t)isr0 + size * i;
    set_idt_entry(&kernel_idt[i], addr,
                  GDT_CODE * sizeof(gdt_entry_t),
                  1, 0);
  }

  __asm__ volatile("lidt (%0)" : : "m"(kernel_idtp));
  enter_v8086_mode(&kernel_tss.esp0);

  show_error_code(2);
}

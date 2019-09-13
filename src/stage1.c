#include "gdt.h"
#include "handlers.h"
#include "interrupts.h"
#include "io.h"
#include "stdint.h"
#include "v8086.h"

extern volatile uint8_t *vesa_framebuffer;
extern unsigned short vesa_pitch;

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

void show_error_code(uint32_t code, int colour)
{
  for (int j = 0; j < 10; j++) {
    for (int i = 0; i < 10; i++) {
      vesa_framebuffer[i + j * vesa_pitch] = colour;
    }
  }

  __asm__ volatile("" : : "c"(code));
  __asm__ volatile("hlt");
  while (1);
}

void pic_eoi(unsigned char irq) {
  if (irq >= 8) {
    /* send to slave too */
    outb(PIC_SLAVE_CMD, PIC_EOI);
  }
  /* always send to master */
  outb(PIC_MASTER_CMD, PIC_EOI);
}

void pic_setup() {
  /* initialise master at offset 0x20 */
  outb(PIC_MASTER_CMD, 0x11); io_wait();
  outb(PIC_MASTER_DATA, 0x20); io_wait();
  outb(PIC_MASTER_DATA, 0x04); io_wait();
  outb(PIC_MASTER_DATA, 0x01); io_wait();
  outb(PIC_MASTER_DATA, 0x00);

  /* initialise slave at offset 0x28 */
  outb(PIC_SLAVE_CMD, 0x11); io_wait();
  outb(PIC_SLAVE_DATA, 0x20); io_wait();
  outb(PIC_SLAVE_DATA, 0x02); io_wait();
  outb(PIC_SLAVE_DATA, 0x01); io_wait();
  outb(PIC_SLAVE_DATA, 0x00);
}

void irq_handler(uint16_t num)
{
  if (num > 0) {
    show_error_code(num, 1);
  }
  pic_eoi(num);
}

void interrupt_handler(isr_stack_t stack)
{
  switch (stack.int_num) {
  case IDT_GP:
    show_error_code(stack.eip, 1);
    break;
  case IDT_PF:
    show_error_code(stack.cs, 5);
    break;
  default:
    show_error_code(0xdead0000 | stack.int_num, 4);
    break;
  }
}

gdtp_t kernel_idtp = {
  sizeof(kernel_idt) - 1,
  kernel_idt,
};

void set_kernel_idt()
{
  for (int i = 0; i < NUM_ISR; i++) {
    uint32_t size = (uint8_t *)isr1 - (uint8_t *)isr0;
    uint32_t addr = (uint32_t)isr0 + size * i;
    set_idt_entry(&kernel_idt[i], addr,
                  GDT_CODE * sizeof(gdt_entry_t),
                  1, 0);
  }

  for (int i = 0; i < NUM_IRQ; i++) {
    uint32_t size = (uint8_t *)irq1 - (uint8_t *)irq0;
    uint32_t addr = (uint32_t)irq0 + size * i;
    set_idt_entry(&kernel_idt[i + IRQ_OFFSET], addr,
                  GDT_CODE * sizeof(gdt_entry_t),
                  1, 0);
  }

  __asm__ volatile("lidt (%0)" : : "m"(kernel_idtp));
}

void _stage1()
{
  set_gdt_entry(&kernel_gdt[GDT_TASK],
                (uint32_t)&kernel_tss,
                sizeof(kernel_tss),
                0x89, 0);
  kernel_tss.tss.ss0 = GDT_SEL(GDT_DATA);
  kernel_tss.tss.iomap_base = sizeof(tss_t);
  __asm__ volatile("ltr %0" : : "r"(GDT_SEL(GDT_TASK)));

  set_kernel_idt();
  pic_setup();

  __asm__ volatile("sti");

  show_error_code(0, 2);
}

#include "gdt.h"
#include "handlers.h"
#include "interrupts.h"
#include "io.h"
#include "stdint.h"
#include "v8086.h"

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

static inline uint32_t seg_off_to_linear(uint16_t seg, uint16_t off)
{
  return (seg << 4) + off;
}

void v8086_gpf_handler(isr_stack_t *stack)
{
  uint8_t *addr = (uint8_t *)seg_off_to_linear(stack->cs, stack->eip);
  switch (*addr) {
  case 0x9c: /* pushf */
    {
      stack->esp -= 2;
      uint16_t *st = (uint16_t *)stack->esp;
      st[0] = stack->eflags & 0xffff;
      stack->eip += 1;
      return;
    }
  case 0x9d: /* popf */
    {
      uint16_t *st = (uint16_t *)stack->esp;
      stack->esp += 2;
      stack->eflags = st[0] | EFLAGS_VM;
      stack->eip += 1;
      return;
    }
  case 0xcf: /* iret */
    {
      uint16_t *st = (uint16_t *)stack->esp;

      /* final iret from v8086 */
      if (stack->esp == V8086_STACK_BASE) {
        v8086_exit(stack);
        return;
      }

      stack->esp += 6;

      stack->eip = st[0];
      stack->cs = st[1];
      stack->eflags = (stack->eflags & 0xffff0000) | st[2];
      return;
    }
  case 0xfa: /* cli */
    stack->eip += 1;
    stack->eflags &= ~EFLAGS_IF;
    return;
  case 0xfb: /* sti */
    stack->eip += 1;
    stack->eflags |= EFLAGS_IF;
    break;
  default:
    __asm__ volatile("" : : "c"(stack));
    while(1);
  }
}

void interrupt_handler(isr_stack_t stack)
{
  int v8086 = stack.eflags & EFLAGS_VM;
  if (v8086) {
    __asm__
      ("mov $0x10, %ax\n"
       "mov %ax, %ds\n"
       "mov %ax, %es\n"
       "mov %ax, %fs\n"
       "mov %ax, %gs\n");
  }

  if (v8086) {
    switch (stack.int_num) {
    case IDT_GP:
      v8086_gpf_handler(&stack);
      return;
    }

    if (stack.int_num >= IDT_IRQ) {
      int irq = stack.int_num - IDT_IRQ;

      if (irq == 0) return;

      /* let the BIOS handle this interrupt */
      stack.esp -= 6;
      uint16_t *st = (uint16_t *)stack.esp;
      st[0] = stack.eip;
      st[1] = stack.cs;
      st[2] = stack.eflags;

      int mapped = irq < 8 ? irq + 8 : irq + 0x68;
      struct { uint16_t off; uint16_t seg; } *iv = 0;
      stack.eip = iv[mapped].off;
      stack.cs = iv[mapped].seg;

      return;
    }
  }

  if (stack.int_num >= IDT_IRQ) {
    int irq = stack.int_num - IDT_IRQ;

    pic_eoi(irq);
    return;
  }

  __asm__ volatile("" : : "c"(&stack));
  while(1);
}

gdtp_t kernel_idtp = {
  sizeof(kernel_idt) - 1,
  kernel_idt,
};

void set_kernel_idt()
{
  for (int i = 0; i < IDT_NUM_ENTRIES; i++) {
    idt_entry_t *entry = &kernel_idt[i];
    entry->offset_low = 0;
    entry->offset_high = 0;
    entry->segment = 0;
    entry->flags = 0;
  }

  uint32_t size = (uint8_t *)isr1 - (uint8_t *)isr0;

  /* isr */
  for (int i = 0; i < NUM_ISR; i++) {
    uint32_t addr = (uint32_t)isr0 + size * i;
    set_idt_entry(&kernel_idt[i], addr,
                  GDT_CODE * sizeof(gdt_entry_t),
                  1, 0);
  }

  /* irq */
  for (int i = 0; i < NUM_IRQ; i++) {
    uint32_t addr = (uint32_t)isr32 + size * i;
    set_idt_entry(&kernel_idt[i + IDT_IRQ], addr,
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
  for (unsigned int i = 0; i < sizeof(kernel_tss.iomap); i++) {
    kernel_tss.iomap[i] = 0;
  }
  __asm__ volatile("ltr %0" : : "r"(GDT_SEL(GDT_TASK)));

  set_kernel_idt();
  pic_setup();

  regs16_t regs = { 0, 0, 0, 0, 0, 0, 0, 0 };
  v8086_enter(0x10, &regs);

  __asm__ volatile("" : : "a"(&regs));
  while(1);
}

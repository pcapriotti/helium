#include "debug.h"
#include "stage1.h"
#include "interrupts.h"
#include "io.h"
#include "stdint.h"
#include "main.h"

volatile uint16_t *vga_text = (uint16_t *)0xb8000;

static int vgax = 0, vgay = 0;
static int enable_tracing = 0;

void debug_str(const char *msg)
{
  volatile uint16_t *p = vga_text + vgax + vgay * 80;
  char c;
  while ((c = *msg++)) {
    if (c == '\n') {
      vgay++;
      vgax = 0;
    }
    else if (vgax < 80) {
      *p++ = 0x700 | c;
      vgax++;
    }
  }
  if (vgay == 25) {
    for (int i = 0; i < 80 * 24; i++) {
      vga_text[i] = vga_text[i + 80];
    }
    for (int i = 0; i < 80; i++) {
      vga_text[80 * 24 + i] = 0;
    }
    vgay--;
  }
}

void debug_byte(uint8_t x)
{
  int d1 = x >> 4;
  int d2 = x & 0xf;
  char msg[] = {
    (char)(d1 > 9 ? 'a' + d1 - 10 : '0' + d1),
    (char)(d2 > 9 ? 'a' + d2 - 10 : '0' + d2),
    0
  };
  debug_str(msg);
}

void text_panic(const char *msg)
{
  debug_str(msg);
  __asm__ volatile("hlt");
  while(1);
}

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
  case 0xcd: /* int */
    {
      ptr16_t *handlers = 0;
      ptr16_t iv = handlers[addr[1]];

      stack->esp -= 6;
      uint16_t *st = (uint16_t *)stack->esp;
      st[0] = stack->eip + 2;
      st[1] = stack->cs;
      st[2] = stack->eflags;

      stack->cs = iv.segment;
      stack->eip = iv.offset;
      return;
    }
  default:
    debug_str("Unimplemented v8086 opcode ");
    debug_byte(*addr);
    debug_str("\n");
    text_panic("Panic");
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

    debug_str("Exception in v8086: ");
    debug_byte(stack.int_num);
    debug_str("\n");
  }

  if (stack.int_num >= IDT_IRQ) {
    int irq = stack.int_num - IDT_IRQ;

    pic_eoi(irq);
    return;
  }

  debug_str("Unhandled exception: ");
  debug_byte(stack.int_num);
  debug_str("\n");

  debug_str("flags: ");
  debug_byte(stack.eflags >> 24);
  debug_byte(stack.eflags >> 16);
  debug_byte(stack.eflags >> 8);
  debug_byte(stack.eflags);
  debug_str(" eip: ");
  debug_byte(stack.eip >> 24);
  debug_byte(stack.eip >> 16);
  debug_byte(stack.eip >> 8);
  debug_byte(stack.eip);
  debug_str(" cs: ");
  debug_byte(stack.cs >> 8);
  debug_byte(stack.cs);
  debug_str("\n");
  text_panic("Panic");
}

gdtp_t kernel_idtp = {
  sizeof(kernel_idt) - 1,
  kernel_idt,
};

/* Create code for an isr stub */
void isr_assemble(isr_t *isr, uint8_t number)
{
  int offset = 4;
  if (number == 8 || number == 10 || number == 11 || number == 12 ||
      number == 13 || number == 14 || number == 17) {
    offset = 0;
  }

  uint8_t *p = isr->code;
  *p++ = 0x83; /* sub */
  *p++ = 0xec; /* esp */
  *p++ = offset;

  *p++ = 0x6a; /* push */
  *p++ = number;

  *p++ = 0xe9; /* rel jump */
  *((int32_t *) p) = (int32_t)isr_generic - (int32_t)(p + 4);
}

__asm__
("isr_generic:"
 "cli\n"
 "pusha\n"
 "call interrupt_handler\n"
 "popa\n"
 "add $8, %esp\n"
 "iret\n");

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

typedef uint8_t sector_t[512];
typedef struct {
  unsigned int sectors_per_track;
  unsigned int tracks_per_cylinder;
  unsigned int num_cylinders;
} drive_geometry_t;

#define SECTORS_PER_TRACK 18
#define TRACKS_PER_CYLINDER 2

void sector_to_chs(drive_geometry_t *geom,
                   unsigned int sector,
                   unsigned int *c,
                   unsigned int *h,
                   unsigned int *s)
{
  unsigned int track = sector / geom->sectors_per_track;
  *s = sector % geom->sectors_per_track + 1;
  *h = track % geom->tracks_per_cylinder;
  *c = track / geom->tracks_per_cylinder;
}

int get_drive_geometry(int drive, drive_geometry_t *geom)
{
  regs16_t regs;
  regs.ax = 0x0800;
  regs.dx = drive;
  regs.es = 0;
  regs.di = 0;

  int flags = bios_int(0x13, &regs);

  int err = flags & EFLAGS_CF;
  if (err) return -1;

  geom->num_cylinders = 1 + ((regs.cx >> 8) | ((regs.cx << 2) & 0x300));
  geom->tracks_per_cylinder = 1 + (regs.dx >> 8);
  geom->sectors_per_track = regs.cx & 0x3f;

  return 0;
}

void load_kernel(int drive)
{
  uint32_t kernel_size = _kernel_end - _kernel_start;
  uint32_t lba0 = _stage1_end - _stage0_start;
  uint32_t lba1 = lba0 + kernel_size;

  int sector0 = lba0 / sizeof(sector_t);
  int sector1 = (lba1 + sizeof(sector_t) - 1) / sizeof(sector_t);

  uint32_t *dest = (uint32_t *)_kernel_start;
  uint32_t *buffer = (uint32_t *)V8086_HEAP;
  regs16_t regs;

  drive_geometry_t geom;
  get_drive_geometry(drive, &geom);

  int sector = sector0;
  while (sector < sector1) {
    int track_end = sector + 1 +
      (sector + SECTORS_PER_TRACK - 1) %
      SECTORS_PER_TRACK;
    if (track_end >= sector1) track_end = sector1;
    int num_sectors = track_end - sector;

    unsigned int c, h, s;
    sector_to_chs(&geom, sector, &c, &h, &s);

    /* read sectors from disk */
    regs.ax = 0x0200 | num_sectors;
    regs.bx = (uint32_t) buffer;
    regs.cx = (s & 0x3f) | (c << 8);
    regs.dx = (h << 8) | (drive & 0xff);
    regs.es = 0;

    int err = bios_int(0x13, &regs) & EFLAGS_CF;

    if (err) {
      text_panic("Could not load kernel from disk");
    }

    unsigned int num_words = num_sectors * sizeof(sector_t) / 4;
    for (unsigned int i = 0; i < num_words; i++) {
      dest[i] = buffer[i];
    }

    sector = track_end;
  }
}

typedef struct {
  uint32_t eip, cs, eflags, sp, ss;
  uint32_t es, ds, fs, gs;
} __attribute__((packed)) v8086_stack_t;

/* The following function enters virtual-8086 mode with the given 16
bit register configuration. It works by taking a stack parameter,
populating it with values that would be pushed before an interrupt
from v8086 to protected mode, and issuing an iret instruction, which
will jump to the given entry point. The stack parameter can be passed
by the caller uninitialised, and is just there to make it easy to set
up the stack. */
uint32_t v8086_enter(regs16_t *regs, ptr16_t entry, v8086_stack_t stack)
{
  isr_stack_t *ctx;

  /* set up the stack for iret */
  stack.es = regs->es;
  stack.ds = regs->ds;
  stack.fs = regs->fs;
  stack.gs = regs->gs;
  stack.eflags = EFLAGS_VM;
  stack.ss = 0;
  stack.sp = V8086_STACK_BASE;
  stack.eip = entry.offset;
  stack.cs = entry.segment;

  __asm__ volatile
    ( /* save current stack pointer in tss */
     "mov %%esp, %0\n"

     /* set up registers */
     "mov 0x0(%3), %%ax\n"
     "mov 0x2(%3), %%bx\n"
     "mov 0x4(%3), %%cx\n"
     "mov 0x6(%3), %%dx\n"
     "mov 0x8(%3), %%di\n"
     "mov 0xa(%3), %%bp\n"

     /* adjust the stack and jump to v8086 mode */
     "mov %2, %%esp\n"
     "iret\n"

     /* control will return here from the v8086 #GP handler after the
     v8086 call stack has underflowed */
     "v8086_exit:\n"

     /* save context */
     "mov 4(%%esp), %%edx\n"

     /* restore stack */
     "mov %0, %%esp\n"

     /* return context */
     "mov %%edx, %1\n"

     : "=m"(kernel_tss.tss.esp0), "=r"(ctx)
     : "X"(&stack), "r"(regs)
     : "%eax", "%ebx", "%ecx", "%edx", "%edi", "%ebp");

  /* update regs structure */
  regs->ax = ctx->eax;
  regs->bx = ctx->ebx;
  regs->cx = ctx->ecx;
  regs->dx = ctx->edx;
  regs->di = ctx->edi;
  regs->bp = ctx->ebp;
  regs->es = ctx->es;
  regs->ds = ctx->ds;
  regs->fs = ctx->fs;
  regs->gs = ctx->gs;

  return ctx->eflags;
}

int bios_int(uint32_t interrupt, regs16_t *regs)
{
  v8086_stack_t stack;
  ptr16_t *handlers = 0; /* bios IVT */
  return v8086_enter(regs, handlers[interrupt], stack);
}

void _stage1(uint32_t drive)
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

  /* set text mode */
  regs16_t regs = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  bios_int(0x10, &regs);

  /* hide cursor */
  regs.ax = 0x0100;
  regs.cx = 0x2000;
  bios_int(0x10, &regs);

  load_kernel(drive);

  main();
}

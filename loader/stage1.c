#include "debug.h"
#include "interrupts.h"
#include "io.h"
#include "stage1.h"

#include "ext2/ext2.h"

#include <string.h>

uint8_t *_kernel_start = (uint8_t *)0x100000;
static int vgax = 0, vgay = 0;

static uint8_t heap[8192];
static uint8_t *heapp = heap;

void *loader_kmalloc(size_t sz) {
  void *ret = heapp;
  heapp += sz;
  return ret;
}
void loader_kfree(void *p) { }

void text_panic(const char *msg)
{
  debug_str("Panic: ");
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

int v8086_tracing = 0;

void v8086_debug_dump(const char *s, isr_stack_t *stack)
{
  debug_str("[v8086] ");
  debug_str(s);
  debug_str(" at ");
  debug_byte(stack->cs >> 8);
  debug_byte(stack->cs);
  debug_str(":");
  debug_byte(stack->eip >> 8);
  debug_byte(stack->eip);
  debug_str(" sp ");
  debug_byte(stack->ss >> 8);
  debug_byte(stack->ss);
  debug_str(":");
  debug_byte(stack->esp >> 8);
  debug_byte(stack->esp);
  debug_str("\n ");
  uint8_t *addr = (uint8_t *)seg_off_to_linear(stack->cs, stack->eip);
  for (int i = 0; i < 15; i++) {
    debug_str(" ");
    debug_byte(addr[i]);
  }
  uint16_t *st = (uint16_t *)seg_off_to_linear(stack->ss, stack->esp);
  debug_str("\n");
  debug_str("  stack:");
  for (int i = 0; i < 8; i++) {
    debug_str(" ");
    debug_byte(st[i] >> 8);
    debug_byte(st[i]);
  }
  debug_str("\n");
}

void v8086_gpf_handler(isr_stack_t *stack)
{
  uint8_t *addr = (uint8_t *)seg_off_to_linear(stack->cs, stack->eip);
  int op32 = 0;

  if (*addr == 0x66) {
    if (v8086_tracing) debug_str("[v8086] opsize\n");
    addr++;
    stack->eip++;
  }
  else if (*addr == 0x67) {
    if (v8086_tracing) debug_str("[v8086] adsize\n");
    addr++;
    stack->eip++;
  }

  switch (*addr) {
  case 0x9c: /* pushf */
    if (v8086_tracing) v8086_debug_dump("pushf", stack);
    if (op32) {
      stack->esp -= 4;
      uint32_t *st = (uint32_t *) stack->esp;
      st[0] = stack->eflags & 0xdff;
      stack->eip += 1;
    }
    else {
      stack->esp -= 2;
      uint16_t *st = (uint16_t *) stack->esp;
      st[0] = stack->eflags & 0xffff;
      stack->eip += 1;
    }
    return;
  case 0x9d: /* popf */
    if (v8086_tracing) v8086_debug_dump("popf", stack);
    if (op32) {
      uint32_t *st = (uint32_t *)stack->esp;
      stack->esp += 4;
      stack->eflags = st[0] | EFLAGS_VM;
      stack->eip += 1;
    }
    else {
      uint16_t *st = (uint16_t *)stack->esp;
      stack->esp += 2;
      stack->eflags = st[0] | EFLAGS_VM;
      stack->eip += 1;
    }
    return;
  case 0xcf: /* iret */
    if (v8086_tracing) v8086_debug_dump("iret", stack);
    {
      uint16_t *st = (uint16_t *)stack->esp;

      /* final iret from v8086 */
      if (stack->esp >= V8086_STACK_BASE) {
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
    if (v8086_tracing) v8086_debug_dump("cli", stack);
    stack->eip += 1;
    stack->eflags &= ~EFLAGS_IF;
    return;
  case 0xfb: /* sti */
    if (v8086_tracing) v8086_debug_dump("sti", stack);
    stack->eip += 1;
    stack->eflags |= EFLAGS_IF;
    return;
  case 0xcd: /* int */
    if (v8086_tracing) v8086_debug_dump("int", stack);
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
    text_panic("v8086");
  }
}

void interrupt_handler(isr_stack_t *stack)
{
  int v8086 = stack->eflags & EFLAGS_VM;
  if (v8086) {
    switch (stack->int_num) {
    case IDT_GP:
      v8086_gpf_handler(stack);
      return;
    }

    if (stack->int_num >= IDT_IRQ) {
      int irq = stack->int_num - IDT_IRQ;

      /* let the BIOS handle this interrupt */
      stack->esp -= 6;
      uint16_t *st = (uint16_t *)stack->esp;
      st[0] = stack->eip;
      st[1] = stack->cs;
      st[2] = stack->eflags;

      int mapped = irq < 8 ? irq + 8 : irq + 0x68;
      struct { uint16_t off; uint16_t seg; } *iv = 0;
      stack->eip = iv[mapped].off;
      stack->cs = iv[mapped].seg;

      return;
    }

    debug_str("Exception in v8086: ");
    debug_byte(stack->int_num);
    debug_str("\n");
  }

  debug_str("Unhandled exception: ");
  debug_byte(stack->int_num);
  debug_str("\n");

  debug_str("flags: ");
  debug_byte(stack->eflags >> 24);
  debug_byte(stack->eflags >> 16);
  debug_byte(stack->eflags >> 8);
  debug_byte(stack->eflags);
  debug_str(" eip: ");
  debug_byte(stack->eip >> 24);
  debug_byte(stack->eip >> 16);
  debug_byte(stack->eip >> 8);
  debug_byte(stack->eip);
  debug_str(" cs: ");
  debug_byte(stack->cs >> 8);
  debug_byte(stack->cs);
  debug_str("\n");
  text_panic("interrupt_handler");
}

gdtp_t kernel_idtp = {
  sizeof(kernel_idt) - 1,
  kernel_idt,
};

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

__asm__
("isr_generic:"
 "pusha\n"
 "mov $0x10, %ax\n"
 "mov %ax, %ds\n"
 "mov %ax, %es\n"
 "mov %ax, %fs\n"
 "mov %ax, %gs\n"
 "push %esp\n"
 "call interrupt_handler\n"
 "add $4, %esp\n"
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
  regs.eax = 0x0800;
  regs.edx = drive;
  regs.es = 0;
  regs.edi = 0;

  int flags = bios_int(0x13, &regs);

  int err = flags & EFLAGS_CF;
  if (err) return -1;

  uint16_t cx = regs.ecx & 0xffff;
  geom->num_cylinders = 1 + ((cx >> 8) | ((cx << 2) & 0x300));
  geom->tracks_per_cylinder = 1 + ((regs.edx >> 8) & 0xff);
  geom->sectors_per_track = cx & 0x3f;

  return 0;
}

void load_kernel(int drive, uint32_t lba0, size_t kernel_size)
{
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
    int track_end = sector + geom.sectors_per_track -
      sector % geom.sectors_per_track;
    if (track_end >= sector1) track_end = sector1;
    int num_sectors = track_end - sector;

    unsigned int c, h, s;
    sector_to_chs(&geom, sector, &c, &h, &s);

    /* read sectors from disk */
    regs.eax = 0x0200 | num_sectors;
    regs.ebx = (uint32_t) buffer;
    regs.ecx = (s & 0x3f) | (c << 8);
    regs.edx = (h << 8) | (drive & 0xff);
    regs.es = 0;

    int err = bios_int(0x13, &regs) & EFLAGS_CF;

    if (err) {
      text_panic("Could not load kernel from disk");
    }

    unsigned int num_words = num_sectors * sizeof(sector_t) / 4;
    for (unsigned int i = 0; i < num_words; i++) {
      *dest++ = buffer[i];
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
will jump to the given entry point. */
uint32_t v8086_enter(regs16_t *regs, v8086_stack_t stack)
{
  isr_stack_t *ctx;
  uint32_t eflags;

  /* set up a stack guard, in case some BIOS code attempts to return
  with a far return instead of an iret */
  {
    uint16_t *st = (uint16_t *) V8086_STACK_BASE;
    st[0] = (uint32_t) (st + 2); /* eip = addr of iret instruction later */
    st[1] = 0; /* cs = 0 */
    st[2] = 0xcf; /* iret */
  }

  /* save flags */
  __asm__ volatile
    (
     "pushf\n"
     "pop %0\n"
     : "=r"(eflags));

  __asm__ volatile
    (/* save registers used here */
     "push %3\n"

     /* save current stack pointer in tss */
     "mov %%esp, %0\n"

     /* adjust the stack */
     "mov %2, %%esp\n"

     /* /\* set up registers *\/ */
     "mov 0x0(%3), %%eax\n"
     "mov 0x4(%3), %%ebx\n"
     "mov 0xc(%3), %%edx\n"
     "mov 0x10(%3), %%edi\n"
     "mov 0x14(%3), %%ebp\n"
     "mov 0x8(%3), %%ecx\n" /* this has to be last */

     /* jump to v8086 mode */
     "iret\n"

     /* control will return here from the v8086 #GP handler after the
     v8086 call stack has underflowed */
     "v8086_exit:\n"

     /* save context */
     "mov 4(%%esp), %1\n"

     /* restore stack */
     "mov %0, %%esp\n"

     /* restore registers */
     "pop %3\n"

     : "=m"(kernel_tss.tss.esp0), "=a"(ctx)
     : "a"(&stack), "c"(regs)
     : "%ebx", "%edx", "%edi", "%ebp");

  /* update regs structure */
  regs->eax = ctx->eax;
  regs->ebx = ctx->ebx;
  regs->ecx = ctx->ecx;
  regs->edx = ctx->edx;
  regs->edi = ctx->edi;
  regs->ebp = ctx->ebp;
  regs->es = ctx->es;
  regs->ds = ctx->ds;
  regs->fs = ctx->fs;
  regs->gs = ctx->gs;

  /* restore flags */
  __asm__ volatile
    ("push %0\n"
     "popf\n"
     :
     : "r"(eflags));

  return ctx->eflags;
}

/* The stack parameter of v8086_enter must be initialised by the
caller to prevent undefined behaviour, so we have to use this little
wrapper that just sets up the stack and passes it by value to
v8086_enter */
uint32_t v8086_set_stack_and_enter(regs16_t *regs, ptr16_t entry)
{
  /* set up the stack for iret */
  v8086_stack_t stack;

  stack.es = regs->es;
  stack.ds = regs->ds;
  stack.fs = regs->fs;
  stack.gs = regs->gs;
  stack.eflags = EFLAGS_VM;
  stack.ss = 0;
  stack.sp = V8086_STACK_BASE;
  stack.eip = entry.offset;
  stack.cs = entry.segment;

  return v8086_enter(regs, stack);
}

int bios_int(uint32_t interrupt, regs16_t *regs)
{
  ptr16_t *handlers = 0; /* bios IVT */
  return v8086_set_stack_and_enter(regs, handlers[interrupt]);
}

typedef struct {
  int drive;
  drive_geometry_t geom;
  size_t part_offset;
} bios_read_info_t;

sector_t bios_read_buffer;

extern int read_debug;

void bios_read_closure(void *data, void *buf,
                       unsigned int offset,
                       unsigned int bytes)
{
  /* kprintf("offset = %#x, bytes = %#x\n", offset, bytes); */
  bios_read_info_t *info = data;
  uint32_t sector0 = info->part_offset + offset / sizeof(sector_t);
  uint32_t sector1 = info->part_offset +
    (offset + bytes + sizeof(sector_t) - 1) / sizeof(sector_t);

  uint8_t *p = buf;
  regs16_t regs;
  uint32_t sector = sector0;
  unsigned int partial_begin = offset % sizeof(sector_t);
  unsigned int partial_end = (offset + bytes) % sizeof(sector_t);
  while (sector < sector1) {
    uint32_t next = sector +
      info->geom.sectors_per_track -
      (sector % info->geom.sectors_per_track);
    if (next > sector1) next = sector1;
    int direct = (sector > sector0 || !partial_begin) &&
      (next < sector1 || !partial_end);
    if (!direct) next = sector + 1;
    uint32_t count = next - sector;

    unsigned int c, h, s;
    sector_to_chs(&info->geom, sector, &c, &h, &s);
    ptr16_t dest = linear_to_ptr16
      (direct ? (uint32_t) p : (uint32_t) &bios_read_buffer);

    regs.eax = 0x200 | (count & 0xff);
    regs.ebx = dest.offset;
    regs.ecx = ((c & 0xff) << 8) | ((c & 0x300) >> 2) | (s & 0x3f) ;
    regs.edx = (h << 8) | (info->drive & 0xff);
    regs.es = dest.segment;

    /* kprintf("dest = %p\n"); */
    /* kprintf("eax = %#x, ebx = %#x, ecx = %#x, edx = %#x, es = %#x\n", */
    /*         regs.eax, regs.ebx, regs.ecx, regs.edx, regs.es); */
    int flags = bios_int(0x13, &regs);
    /* kprintf("  CF = %u\n", flags & EFLAGS_CF); */

    if (direct) {
      p += count * sizeof(sector_t);
    }
    else {
      unsigned int i0 = sector > sector0 ? 0 : partial_begin;
      unsigned int i1 = next < sector1 ? 0 : partial_end;
      for (unsigned int i = i0; i < i1; i++) {
        *p++ = bios_read_buffer[i];
      }
    }

    sector += count;
  }
}

void _stage1(uint32_t drive)
{
  /* set bss to zero */
  for (uint8_t *p = _bss_start;
       p < _bss_end; p++) {
    *p = 0;
  }

  set_gdt_entry(&kernel_gdt[GDT_TASK],
                (uint32_t)&kernel_tss,
                sizeof(kernel_tss),
                0x89, 0);
  kernel_tss.tss.ss0 = GDT_SEL(GDT_DATA);
  kernel_tss.tss.iomap_base = sizeof(tss_t);
  __asm__ volatile("ltr %0" : : "r"(GDT_SEL(GDT_TASK)));

  set_kernel_idt();
  pic_setup();

  /* set text mode */
  regs16_t regs = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  regs.eax = 0x2;
  bios_int(0x10, &regs);

  /* hide cursor */
  regs.eax = 0x0100;
  regs.ecx = 0x2000;
  bios_int(0x10, &regs);

  bios_read_info_t info;
  info.drive = 0x80;
  info.part_offset = 72;
  kprintf("drive = %#x\n", info.drive);
  get_drive_geometry(info.drive, &info.geom);

  fs_t *fs = ext2_new_fs(&bios_read_closure, &info);
  if (!fs) text_panic("fs");
  inode_t *inode = ext2_get_path_inode(fs, "boot/kernel");
  if (inode) {
    inode_t tmp = *inode;
    inode_iterator_t *it = ext2_inode_iterator_new(fs, &tmp);
    while (!ext2_inode_iterator_end(it)) {
      char *buf = ext2_inode_iterator_read(it);
      int len = ext2_inode_iterator_block_size(it);

      char *s = loader_kmalloc(len + 1);
      strncpy(s, buf, len);
      s[len] = '\0';
      kprintf("%s\n", s);
      loader_kfree(s);

      ext2_inode_iterator_next(it);
    }
    loader_kfree(it);
  }
  ext2_free_fs(fs);

  debug_str("Ok.\n");
  while(1);
}

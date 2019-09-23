#include "core/debug.h"
#include "core/elf.h"
#include "core/interrupts.h"
#include "core/io.h"
#include "core/v8086.h"
#include "stage1.h"

#include "ext2/ext2.h"

#include <string.h>

static int vgax = 0, vgay = 0;

static uint8_t heap[8192];
static uint8_t *heapp = heap;

void *loader_kmalloc(size_t sz) {
  void *ret = heapp;
  heapp += sz;
  return ret;
}
void loader_kfree(void *p) { }

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

size_t load_kernel(unsigned int drive, unsigned int part_offset)
{
  bios_read_info_t info;
  info.drive = 0x80 | drive;
  info.part_offset = part_offset;
  get_drive_geometry(info.drive, &info.geom);

  fs_t *fs = ext2_new_fs(&bios_read_closure, &info);
  if (!fs) panic();
  inode_t *inode = ext2_get_path_inode(fs, "boot/kernel");
  if (!inode) {
    ext2_free_fs(fs);
    panic();
  }

  inode_t tmp = *inode;
  inode_iterator_t *it = ext2_inode_iterator_new(fs, &tmp);
  if (ext2_inode_iterator_end(it)) {
    ext2_free_fs(fs);
    loader_kfree(it);
    return 0;
  }

  uint8_t *buf = ext2_inode_iterator_read(it);
  size_t len = ext2_inode_iterator_block_size(it);

  int ret = elf_test(buf, len);

  /* while (!ext2_inode_iterator_end(it)) { */
  /*   char *buf = ext2_inode_iterator_read(it); */
  /*   int len = ext2_inode_iterator_block_size(it); */
  /*   kprintf("kernel block of length %u\n", len); */

  /*   for (int i = 0; i < len; i++) { */
  /*     *dest++ = buf[i]; */
  /*   } */

  /*   ext2_inode_iterator_next(it); */
  /* } */
  loader_kfree(it);
  ext2_free_fs(fs);

  return 0;
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

  size_t ksize = load_kernel(0, 72);
  kprintf("Loaded kernel: %u bytes\n", ksize);

  debug_str("Ok.\n");
  while(1);
}

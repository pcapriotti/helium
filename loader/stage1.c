#include "core/debug.h"
#include "core/gdt.h"
#include "core/elf.h"
#include "core/interrupts.h"
#include "core/io.h"
#include "core/vfs.h"
#include "core/v8086.h"
#include "stage1.h"

#include "ext2/ext2.h"
#include "ext2/ext2_fs.h"

#include <string.h>

static int vgax = 0, vgay = 0;

#define LOADER_HEAP_SIZE 8192
static uint8_t heap[LOADER_HEAP_SIZE];
static uint8_t *heapp = heap;

void *loader_kmalloc(size_t sz) {
  void *ret = heapp;
  heapp += sz;
  if (heapp > heap + LOADER_HEAP_SIZE) panic();
  return ret;
}
void loader_kfree(void *p) { }

/* GDT */


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

void *load_kernel(unsigned int drive, unsigned int part_offset)
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
  vfs_file_t *file = ext2_vfs_file_new(fs, &tmp);
  void *entry = elf_load_exe(file);

  ext2_vfs_file_del(file);
  ext2_free_fs(fs);

  return entry;
}

typedef void (*main_t)();

void _stage1(uint32_t drive)
{
  /* set bss to zero */
  for (uint8_t *p = _bss_start;
       p < _bss_end; p++) {
    *p = 0;
  }

  gdt_set_entry(&kernel_gdt[GDT_TASK],
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

  main_t entry = load_kernel(0, 72);
  kprintf("Jumping to kernel entry point at %p\n", entry);
  for (int i = 0; i < 10; i++) {
    kprintf("%02x ", ((uint8_t *)entry)[i]);
  }
  kprintf("\n");
  entry();

  debug_str("Ok.\n");
  while(1);
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
 "call v8086_interrupt_handler\n"
 "add $4, %esp\n"
 "popa\n"
 "add $8, %esp\n"
 "iret\n"
 ".globl isr_generic");

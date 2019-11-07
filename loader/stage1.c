#include "bios_storage.h"
#include "core/allocator.h"
#include "core/debug.h"
#include "core/gdt.h"
#include "core/elf.h"
#include "core/interrupts.h"
#include "core/io.h"
#include "core/serial.h"
#include "core/storage.h"
#include "core/vfs.h"
#include "core/v8086.h"
#include "stage1.h"

#include "kernel/fs/ext2/ext2.h"
#include "kernel/fs/ext2/ext2_fs.h"

#include <string.h>

static int vgax = 0, vgay = 0;

#define LOADER_HEAP_SIZE 8192
static uint8_t heap[LOADER_HEAP_SIZE];
static uint8_t *heapp = heap;

void *loader_kmalloc(void *data, size_t sz) {
  void *ret = heapp;
  heapp += sz;
  if (heapp > heap + LOADER_HEAP_SIZE) panic();
  return ret;
}
void loader_kfree(void *data, void *p) { }

allocator_t loader_allocator = {
  .alloc = loader_kmalloc,
  .free = loader_kfree,
};

/* GDT */


typedef uint8_t sector_t[512];

#define SECTORS_PER_TRACK 18
#define TRACKS_PER_CYLINDER 2


void *load_kernel(unsigned int drive, unsigned int part_offset)
{
  bios_storage_info_t info;
  info.drive = 0x80 | drive;
  info.part_offset = part_offset;
  get_drive_geometry(info.drive, &info.geom);

  bios_storage.ops_data = &info;

  /* load from an ext2 filesystem */
  vfs_ops_t *vfs_ops = &ext2_vfs_ops;

  vfs_t *vfs = vfs_ops->new(&bios_storage, &loader_allocator);
  if (!vfs) panic();
  vfs_file_t *file = vfs_open(vfs, "boot/kernel");
  if (!file) panic();
  void *entry = elf_load_exe(file);

  vfs_close(vfs, file);
  vfs_del(vfs);

  return entry;
}

typedef void (*main_t)();

void loader_start(uint32_t drive)
{
  /* set bss to zero */
  for (uint8_t *p = _bss_start;
       p < _bss_end; p++) {
    *p = 0;
  }
  serial_init();

  gdt_set_entry(&kernel_gdt[GDT_TASK],
                (uint32_t)&kernel_tss,
                sizeof(kernel_tss),
                0x89, 0);
  kernel_tss.tss.ss0 = GDT_SEL(GDT_DATA);
  kernel_tss.tss.iomap_base = sizeof(tss_t);
  __asm__ volatile("ltr %0" : : "r"(GDT_SEL(GDT_TASK)));

  idt_init();
  pic_init();

  /* set text mode */
  regs16_t regs = {0};
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
  __asm__
    ("call *%0"
     : :
       "r"(entry), "a"(LOADER_MAGIC), "b"(0));

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

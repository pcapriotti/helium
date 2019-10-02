#include "ata.h"
#include "atomic.h"
#include "console.h"
#include "core/gdt.h"
#include "core/interrupts.h"
#include "core/debug.h"
#include "core/io.h"
#include "core/v8086.h"
#include "ext2/ext2.h"
#include "graphics.h"
#include "list.h"
#include "keyboard.h"
#include "kmalloc.h"
#include "mbr.h"
#include "memory.h"
#include "pci.h"
#include "scheduler.h"
#include "timer.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

void hang_system(void);

typedef struct {
  drive_t *drive;
  uint32_t part_offset;
} ata_closure_data_t;

void ata_read_closure(void *data, void *buf,
                      unsigned int offset,
                      unsigned int bytes)
{
  ata_closure_data_t *clo_data = data;
#if ATA_CLOSURE_DEBUG
  kprintf("reading at %#x from drive %u:%u with part offset %#x\n",
          offset, clo_data->drive->channel,
          clo_data->drive->index,
          clo_data->part_offset);
#endif
  ata_read_bytes(clo_data->drive,
                 offset + (clo_data->part_offset << 9),
                 bytes,
                 buf);
}

void on_kb_event(kb_event_t *event)
{
  if (event->pressed && event->printable) {
    kprintf("%c", event->printable);
  }
}

void kmain()
{
  set_gdt_entry(&kernel_gdt[GDT_TASK],
                (uint32_t)&kernel_tss,
                sizeof(kernel_tss),
                0x89, 0);
  __asm__ volatile("lgdt %0" : : "m"(kernel_gdtp));

  kernel_tss.tss.ss0 = GDT_SEL(GDT_DATA);
  kernel_tss.tss.iomap_base = sizeof(tss_t);
  __asm__ volatile("ltr %0" : : "r"(GDT_SEL(GDT_TASK)));

  set_kernel_idt();

  /* add idt entry for syscalls */


  pic_setup();
  /* set text mode */
  regs16_t regs = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  regs.eax = 0x2;
  bios_int(0x10, &regs);

  /* hide cursor */
  regs.eax = 0x0100;
  regs.ecx = 0x2000;
  bios_int(0x10, &regs);

  kprintf("Helium starting\n");

  /* set up a temporary heap in low memory */
  void *heap = (void *)0x500;

  if (timer_init() == -1) panic();
  if (kb_init() == -1) panic();
  sti();

  if (memory_init(heap) == -1) panic();
  if (kmalloc_init(memory_frames) == -1) panic();

  kprintf("entering graphic mode\n");

  /* copy debug console state */
  uint16_t *debug_buf = falloc(80 * 25 * sizeof(uint16_t));
  for (int i = 0; i < 25; i++) {
    int p = 80 * i;
    for (int j = 0; j < 80; j++) {
      debug_buf[p] = vga_text[p];
      p++;
    }
  }

  {
    vbe_mode_t mode;
    mode.width = 800;
    mode.height = 600;
    mode.bpp = 32;
    if (graphics_init(&mode) == -1) panic();
  }

  if (console_init() == -1) panic();

  for (int i = 0; i < 25; i++) {
    int p = console.width * i;
    int q = 80 * i;
    for (int j = 0; j < 80; j++) {
      console.buffer[p++] = debug_buf[q++];
    }
  }
  ffree(debug_buf);
  console_render_buffer();
  console.cur.x = debug_console.x;
  console.cur.y = debug_console.y;
  print_char_function = &console_debug_print_char;

  kprintf("console %dx%d\n",
          console.width, console.height);

  LIST_HEAD(devices);
  pci_scan(&devices);

  kb_grab(on_kb_event);

  kprintf("Ok.\n");

  sched_enable_preemption();

  hang_system();
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
 "call handle_interrupt\n"
 "add $4, %esp\n"
 "popa\n"
 "add $8, %esp\n"
 "iret\n"
 ".globl isr_generic");

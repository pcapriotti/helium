#include "ata.h"
#include "atomic.h"
#include "console.h"
#include "core/gdt.h"
#include "core/interrupts.h"
#include "core/debug.h"
#include "core/io.h"
#include "core/v8086.h"
#include "core/x86.h"
#include "ext2/ext2.h"
#include "graphics.h"
#include "list.h"
#include "keyboard.h"
#include "kmalloc.h"
#include "mbr.h"
#include "memory.h"
#include "paging.h"
#include "pci.h"
#include "scheduler.h"
#include "shell.h"
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

void root_task()
{
  if (graphics_mode.number) {
    kprintf("mode %#x: %ux%u %u bits\n",
            (uint32_t) graphics_mode.number,
            (uint32_t) graphics_mode.width,
            (uint32_t) graphics_mode.height,
            (uint32_t) graphics_mode.bpp);
    kprintf("console %dx%d\n",
            console.width, console.height);
  }

  list_t *devices = pci_scan();
  if (ata_init(devices) == -1) {
    kprintf("ERROR: could not initialise ATA\n");
  }

  serial_printf("spawning shell\n");
  sched_spawn_task(shell_main);

  kprintf("Ok.\n");
}

void kernel_start(void *multiboot_info, uint32_t magic)
{
  gdt_init();
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

  kprintf("Helium starting (magic = %#x)\n", magic);

  /* set up a temporary heap in low memory */
  void *heap = (void *)0x500;

  /* initialise serial port */

  if (timer_init() == -1) panic();
  if (kb_init() == -1) panic();
  sti();

  if (memory_init(heap) == -1) panic();
  if (kmalloc_init() == -1) panic();

  paging_init();

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
    mode.width = 2000;
    mode.height = 1000;
    mode.bpp = 32;
    mode.number = 0;
    if (graphics_init(heap, &mode) == -1) {
      kprintf("ERROR: could not enter graphic mode\n");
    }
    (void) mode;
  }

  if (graphics_mode.number) {
    if (console_init() == -1) panic();
    for (int i = 0; i < 25; i++) {
      int p = console.width * i;
      int q = 80 * i;
      for (int j = 0; j < 80; j++) {
        console.buffer[p++] = debug_buf[q++];
      }
    }
    console.cur.x = debug_console.x;
    console.cur.y = debug_console.y;
    console_render_buffer();
    print_char_function = &console_debug_print_char;
    serial_printf("background task:\n");
    console_start_background_task();
  }
  ffree(debug_buf);

  serial_printf("root task:\n");
  sched_spawn_task(root_task);
  sched_yield();
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

#include "atomic.h"
#include "console/console.h"
#include "console/fbcon.h"
#include "console/textcon.h"
#include "core/gdt.h"
#include "core/interrupts.h"
#include "core/debug.h"
#include "core/io.h"
#include "core/serial.h"
#include "core/v8086.h"
#include "core/x86.h"
#include "drivers/ata/ata.h"
#include "drivers/drivers.h"
#include "drivers/keyboard/keyboard.h"
#include "fs/ext2/ext2.h"
#include "graphics.h"
#include "list.h"
#include "kmalloc.h"
#include "mbr.h"
#include "memory.h"
#include "multiboot.h"
#include "network/network.h"
#include "paging/paging.h"
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

static console_backend_t *console_backend_get(void)
{
  if (graphics_mode.number) {
    return fbcon_backend_get();
  }
  else {
    return textcon_backend_get();
  }
}

void root_task(void)
{
  console_set_fg(0x005799d0);
  kprintf("mode %#x: %ux%u %u bits\n",
          (uint32_t) graphics_mode.number,
          (uint32_t) graphics_mode.width,
          (uint32_t) graphics_mode.height,
          (uint32_t) graphics_mode.bpp);
  kprintf("console %dx%d\n",
          console.width, console.height);
  console_reset_fg();

  drivers_init();
  list_t *devices = pci_scan();

  sched_spawn_task(network_init);

  console_set_fg(0x007c9a59);
  kprintf("Ok.\n");
  console_reset_fg();

  sched_spawn_task(shell_main);
}

void kernel_start(void *multiboot, uint32_t magic)
{
  /* ignore multiboot info unless we come from a multiboot loader */
  if (magic != MB_MAGIC_EAX) multiboot = 0;

  serial_init();
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

  sti();
  if (timer_init() == -1) panic();
  if (kb_init() == -1) panic();

  if (memory_init(multiboot) == -1) panic();

  kprintf("entering graphic mode\n");

  uint16_t *debug_buf = falloc(80 * 25 * sizeof(uint16_t));

  {
    vbe_mode_t mode;
    mode.width = 2000;
    mode.height = 1000;
    mode.bpp = 32;
    mode.number = 0;
    if (graphics_init(&mode, debug_buf) == -1) {
      serial_printf("ERROR: could not enter graphic mode\n");
    }
    (void) mode;
  }

  if (console_init(console_backend_get()) == -1) panic();
  for (int i = 0; i < 25 && i < console.height; i++) {
    int p = console.width * i;
    int q = 80 * i;
    for (int j = 0; j < 80 && j < console.width; j++) {
      char c = debug_buf[q++];
      if (c > 0x20 && c < 0x7f) console.lengths[i] = j + 1;
      console.buffer[p++] = c;
    }
  }

  console.cur.x = debug_console.x;
  console.cur.y = debug_console.y;
  console_render_buffer();
  print_char_function = &console_debug_print_char;
  redraw_screen_function = &console_render_buffer;
  console_start_background_task();
  ffree(debug_buf);

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

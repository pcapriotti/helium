#include "ata.h"
#include "console.h"
#include "debug.h"
#include "ext2/ext2.h"
#include "graphics.h"
#include "list.h"
#include "kmalloc.h"
#include "mbr.h"
#include "memory.h"
#include "pci.h"
#include "stage1.h"
#include "timer.h"

#include <stdint.h>
#include <stddef.h>

void hang_system(void);

void test_read(void *data, void *buf,
               unsigned int offset,
               unsigned int bytes)
{
}

typedef struct {
  list_t head;
  int x;
} test_t;

void main()
{
  __asm__ volatile("sti");

  void *heap = _kernel_low_end;

  if (timer_init() == -1) panic();

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
    if (graphics_init(&mode) == -1) text_panic("graphics");
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
  console.cur.x = debug_console.x;
  console.cur.y = debug_console.y;

  kprintf("console %dx%d\n",
          console.width, console.height);

  LIST_HEAD(devices);
  pci_scan(&devices);

  if (ata_init(&devices) == -1) panic();

  drive_t *drive = ata_get_drive(0);
  if (drive->present) {
    partition_table_t table;
    read_partition_table(drive, table);
    for (int i = 0; i < 4; i++) {
      kprintf("part %u: %#x - %#x\n",
              i, table[i].lba_start,
              table[i].lba_start + table[i].num_sectors);
    }
  }
  else {
    kprintf("no drive 0\n");
  }

  fs_t *fs = ext2_new_fs(test_read, 0);
  ext2_free_fs(fs);

  kprintf("Ok.\n");
  hang_system();
}

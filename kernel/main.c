#include "ata.h"
#include "console.h"
#include "core/debug.h"
#include "ext2/ext2.h"
#include "graphics.h"
#include "list.h"
#include "kmalloc.h"
#include "mbr.h"
#include "memory.h"
#include "pci.h"
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

void main()
{
  kprintf("hello from kernel\n");
  hang_system();

  __asm__ volatile("sti");

  void *heap = _kernel_end;

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
    ata_closure_data_t clo_data;
    clo_data.drive = drive;
    clo_data.part_offset = table[0].lba_start;

    fs_t *fs = ext2_new_fs(ata_read_closure, &clo_data);

    inode_t *inode = ext2_get_path_inode(fs, "hello");
    if (inode) {
      inode_t tmp = *inode;
      inode_iterator_t *it = ext2_inode_iterator_new(fs, &tmp);
      while (!ext2_inode_iterator_end(it)) {
        char *buf = ext2_inode_iterator_read(it);
        int len = ext2_inode_iterator_block_size(it);

        char *s = kmalloc(len + 1);
        strncpy(s, buf, len);
        s[len] = '\0';
        kprintf("%s", s);
        kfree(s);

        ext2_inode_iterator_next(it);
      }
      kfree(it);
    }

    ext2_free_fs(fs);

  }
  else {
    kprintf("no drive 0\n");
  }


  kprintf("Ok.\n");
  hang_system();
}

#include "bios_storage.h"
#include "core/storage.h"
#include "core/v8086.h"
#include "core/x86.h"
#include "core/util.h"

#define SECTOR_ALIGNMENT 9
#define SECTOR_SIZE (1UL << 9)

extern storage_ops_t bios_storage_ops;

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


static void sector_to_chs(drive_geometry_t *geom,
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

int bios_ops_read_unaligned(void *data, void *buf, void *scratch,
                            uint64_t offset, uint32_t bytes)
{
  return storage_read_unaligned_helper(&bios_storage_ops, data, buf, scratch,
                               offset, bytes);
}

int bios_read(int drive, drive_geometry_t *geom, unsigned sector, unsigned count, void *buf)
{
  regs16_t regs;

  unsigned int c, h, s;
  sector_to_chs(geom, sector, &c, &h, &s);
  ptr16_t dest = linear_to_ptr16((size_t) buf);

  regs.eax = 0x200 | (count & 0xff);
  regs.ebx = dest.offset;
  regs.ecx = ((c & 0xff) << 8) | ((c & 0x300) >> 2) | (s & 0x3f) ;
  regs.edx = (h << 8) | (drive & 0xff);
  regs.es = dest.segment;

  int flags = bios_int(0x13, &regs);

  return (flags & EFLAGS_CF) ? -1 : 0;
}

int bios_ops_read(void *data, void *buf,
                  uint64_t offset, uint32_t bytes)
{
  assert(IS_ALIGNED(offset, SECTOR_ALIGNMENT));
  assert(IS_ALIGNED(bytes, SECTOR_ALIGNMENT));

  bios_storage_info_t *info = data;
  unsigned sector0 = info->part_offset + (offset >> SECTOR_ALIGNMENT);
  unsigned sector1 = sector0 + (bytes >> SECTOR_ALIGNMENT);

  unsigned sector = sector0;
  /* read at most one track at a time */
  while (sector < sector1) {
    unsigned next = sector +
      info->geom.sectors_per_track -
      (sector % info->geom.sectors_per_track);
    if (next > sector1) next = sector1;
    int ret = bios_read(info->drive, &info->geom, sector, next - sector, buf);
    if (ret == -1) return -1;
    sector = next;
  }

  return 0;
}

storage_ops_t bios_storage_ops = {
  .read_unaligned = bios_ops_read_unaligned,
  .read = bios_ops_read,
  /* write not supported */
  .write_unaligned = 0,
  .write = 0,
  .alignment = SECTOR_ALIGNMENT,
};

storage_t bios_storage = {
  .ops = &bios_storage_ops,
  .ops_data = 0,
};

#include "bios_storage.h"
#include "core/storage.h"
#include "core/v8086.h"
#include "core/x86.h"
#include "core/util.h"

#define SECTOR_ALIGNMENT 9
#define SECTOR_SIZE (1UL << 9)

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

  bios_storage_info_t *info = data;
  uint32_t sector0 = info->part_offset + (offset >> SECTOR_ALIGNMENT);
  uint32_t sector1 = ROUND(offset + bytes, SECTOR_ALIGNMENT);

  uint8_t *p = buf;
  regs16_t regs;
  uint32_t sector = sector0;
  unsigned int partial_begin = offset % SECTOR_SIZE;
  unsigned int partial_end = (offset + bytes) % SECTOR_SIZE;
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
      (direct ? (uint32_t) p : (uint32_t) scratch);

    regs.eax = 0x200 | (count & 0xff);
    regs.ebx = dest.offset;
    regs.ecx = ((c & 0xff) << 8) | ((c & 0x300) >> 2) | (s & 0x3f) ;
    regs.edx = (h << 8) | (info->drive & 0xff);
    regs.es = dest.segment;

    int flags = bios_int(0x13, &regs);

    if (direct) {
      p += count * SECTOR_SIZE;
    }
    else {
      unsigned int i0 = sector > sector0 ? 0 : partial_begin;
      unsigned int i1 = next < sector1 ? 0 : partial_end;
      for (unsigned int i = i0; i < i1; i++) {
        *p++ = ((uint8_t *)scratch)[i];
      }
    }

    sector += count;
  }

  return 0;
}

int bios_ops_read(void *data, void *buf,
                    uint64_t offset, uint32_t bytes)
{
  assert(IS_ALIGNED(offset, SECTOR_ALIGNMENT));
  assert(IS_ALIGNED(bytes, SECTOR_ALIGNMENT));
  return bios_ops_read_unaligned(data, buf, 0, offset, bytes);
}

storage_ops_t bios_storage_ops = {
  .read_unaligned = bios_ops_read_unaligned,
  .read = bios_ops_read,
  /* write not supported */
  .write_unaligned = 0,
  .write = 0,
};

storage_t bios_storage = {
  .ops = &bios_storage_ops,
  .ops_data = 0,
  .alignment = SECTOR_ALIGNMENT,
};

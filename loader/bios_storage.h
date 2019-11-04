#ifndef BIOS_STORAGE_H
#define BIOS_STORAGE_H

#include <stdint.h>
#include <stddef.h>

typedef struct drive_geometry {
  unsigned int sectors_per_track;
  unsigned int tracks_per_cylinder;
  unsigned int num_cylinders;
} drive_geometry_t;

typedef struct bios_storage_info {
  int drive;
  drive_geometry_t geom;
  size_t part_offset;
} bios_storage_info_t;

int get_drive_geometry(int drive, drive_geometry_t *geom);

struct storage;

extern struct storage bios_storage;

#endif /* BIOS_STORAGE_H */

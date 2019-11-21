#ifndef MAPPING_H
#define MAPPING_H

#include <stdint.h>
#include <stddef.h>

#include "core/storage.h"

typedef struct storage_mapping {
  storage_t *storage;
  storage_offset_t offset;

  /* internal buffer */
  void *buf;
  size_t buf_size;
  storage_offset_t buf_offset;
} storage_mapping_t;

void storage_mapping_init(storage_mapping_t *map,
                          storage_t *storage,
                          storage_offset_t offset,
                          void *buf,
                          size_t buf_size);

void *storage_mapping_read(storage_mapping_t *map,
                           storage_offset_t offset,
                           size_t size);

int storage_mapping_write(storage_mapping_t *map,
                          void *buf,
                          storage_offset_t offset,
                          size_t size);

#define storage_mapping_read_item(map, index, ty) \
  ((ty *) storage_mapping_read(map, (index) * sizeof(ty), sizeof(ty)))

#endif /* MAPPING_H */
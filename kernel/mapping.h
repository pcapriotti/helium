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

void *storage_mapping_get(storage_mapping_t *map,
                          size_t offset, size_t size);

#endif /* MAPPING_H */

#include "mapping.h"
#include "core/allocator.h"
#include "core/debug.h"
#include "core/util.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#define DEBUG_LOCAL 0

#ifndef _HELIUM
#if DEBUG_LOCAL
#include <stdio.h>
#define serial_printf printf
#endif
#endif

static int storage_mapping_fetch(storage_mapping_t *map,
                                 storage_offset_t offset)
{
#if DEBUG_LOCAL
  serial_printf("[storage_mapping] fetching offset: %" PRIu64 "\n", offset);
#endif
  size_t start = ALIGN64(offset, storage_sector_size(map->storage));
  map->buf_offset = start;
  return storage_read(map->storage,
                      map->buf,
                      map->offset + start,
                      map->buf_size);
}

static int storage_mapping_in_offset(storage_mapping_t *map,
                                     storage_offset_t offset,
                                     size_t size)
{
  return offset >= (size_t) map->buf_offset &&
         offset + size <= map->buf_offset + map->buf_size;
}

void storage_mapping_init(storage_mapping_t *map,
                          storage_t *storage,
                          storage_offset_t offset,
                          void *buf,
                          size_t buf_size)
{
  assert(ALIGNED64(offset, storage_sector_size(storage)));
  assert(ALIGNED(buf_size, storage_sector_size(storage)));

  map->storage = storage;
  map->offset = offset;
  map->buf = buf;
  map->buf_size = buf_size;

  /* do an initial fetch at offset 0 */
  storage_mapping_fetch(map, 0);
}

void storage_mapping_reset(storage_mapping_t *map,
                           storage_offset_t offset)
{
  map->offset = offset;
  storage_mapping_fetch(map, 0);
}

/*
  Retrieve a portion of the underlying storage. The offset is
  relative to the beginning of the mapping. The returned buffer is
  only guaranteed to be valid until the next storage_mapping call.
*/
void *storage_mapping_read(storage_mapping_t *map,
                           storage_offset_t offset,
                           size_t size)
{
#if DEBUG_LOCAL
  serial_printf("[storage_mapping] read offset: %" PRIu64 " size: %lu\n",
                offset, size);
  serial_printf("[storage mapping] map: %p, buf_offset: %" PRIu64
                " buf_size: %lu\n",
                map, map->buf_offset, map->buf_size);
#endif
  assert(size <= map->buf_size);

  if (!storage_mapping_in_offset(map, offset, size)) {
    if (storage_mapping_fetch(map, offset) == -1)
      return 0;
  }

  assert(storage_mapping_in_offset(map, offset, size));

  void *ret = map->buf + offset - map->buf_offset;

  return ret;
}

/*
  Write the given buffer to storage. The buffer provided must be a
  pointer returned by `storage_mapping_get`, and it must be valid.
*/
int storage_mapping_put(storage_mapping_t *map,
                        void *buf, size_t size)
{
#if DEBUG_LOCAL
  serial_printf("[storage mapping] put size: %lu, buf_size: %lu\n",
                size, map->buf_size);
#endif
  assert(buf >= map->buf);
  assert(buf + size <= map->buf + map->buf_size);

  size_t offset = buf - map->buf;
  size_t start = ALIGN64(offset, storage_sector_size(map->storage));
  size_t end = ALIGN64_UP(offset + size, storage_sector_size(map->storage));

  assert(end - start <= map->buf_size);

  return storage_write(map->storage,
                       map->buf + start,
                       map->offset + start,
                       end - start);
}

/*
  Write an arbitrary memory area to storage. This involves copying the
  memory to the interal storage.
*/
int storage_mapping_write(storage_mapping_t *map,
                          void *buf,
                          storage_offset_t offset,
                          size_t size)
{
  void *buf2 = storage_mapping_read(map, offset, size);
  memcpy(buf2, buf, size);
  return storage_mapping_put(map, buf2, size);
}

storage_mapping_t *storage_mapping_new(allocator_t *allocator,
                                       storage_t *storage,
                                       storage_offset_t offset,
                                       size_t buf_size)
{
  storage_mapping_t *map = allocator_alloc(allocator,
                                           sizeof(storage_mapping_t));
  buf_size = ALIGN_UP(buf_size, storage_sector_size(storage));
  void *buf = allocator_alloc(allocator, buf_size);
  storage_mapping_init(map, storage, offset, buf, buf_size);
  return map;
}

void storage_mapping_del(storage_mapping_t *map,
                         allocator_t *allocator)
{
  allocator_free(allocator, map->buf);
  allocator_free(allocator, map);
}

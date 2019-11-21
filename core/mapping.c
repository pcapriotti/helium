#include "mapping.h"
#include "core/util.h"

#include <assert.h>
#include <string.h>

static int storage_mapping_fetch(storage_mapping_t *map,
                                 storage_offset_t offset)
{
  size_t start = ALIGNED(offset, storage_alignment(map->storage));
  return storage_read(map->storage,
                      map->buf,
                      map->offset + start,
                      map->buf_size);
  map->buf_offset = start;
}

static int storage_mapping_in_offset(storage_mapping_t *map,
                                     storage_offset_t offset,
                                     size_t size)
{
  return offset >= (size_t) map->buf_offset &&
         offset + size < map->buf_offset + map->buf_size;
}

void storage_mapping_init(storage_mapping_t *map,
                          storage_t *storage,
                          storage_offset_t offset,
                          void *buf,
                          size_t buf_size)
{
  assert(IS_ALIGNED(offset, storage_alignment(storage)));
  assert(IS_ALIGNED(buf_size, storage_alignment(storage)));

  map->storage = storage;
  map->offset = offset;
  map->buf = buf;
  map->buf_size = buf_size;

  /* do an initial fetch at offset 0 */
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
  assert(size <= map->buf_size);

  if (!storage_mapping_in_offset(map, offset, size)) {
    if (storage_mapping_fetch(map, offset) == -1)
      return 0;
  }

  assert(storage_mapping_in_offset(map, offset, size));

  return map->buf + offset - map->buf_offset;
}

/*
  Write the given buffer to storage. The buffer provided must be a
  pointer returned by `storage_mapping_get`, and it must be valid.
*/
int storage_mapping_put(storage_mapping_t *map,
                        void *buf, size_t size)
{
  assert(buf >= map->buf);
  assert(buf + size < map->buf + map->buf_size);

  size_t offset = buf - map->buf;
  size_t start = ALIGNED(offset, storage_alignment(map->storage));
  size_t end = ALIGNED_UP(offset + size, storage_alignment(map->storage));

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

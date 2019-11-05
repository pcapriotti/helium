#include "core/storage.h"
#include "core/util.h"

#include <assert.h>
#include <string.h>

int storage_read(storage_t *storage, void *buf,
                 uint64_t offset, uint32_t bytes)
{
  return storage->ops->read(storage->ops_data, buf, offset, bytes);
}

int storage_read_unaligned(storage_t *storage, void *buf, void *scratch,
                           uint64_t offset, uint32_t bytes)
{
  return storage->ops->read_unaligned
    (storage->ops_data, buf, scratch, offset, bytes);
}

int storage_write(storage_t *storage, void *buf,
                  uint64_t offset, uint32_t bytes)
{
  return storage->ops->write(storage->ops_data, buf, offset, bytes);
}

int storage_write_unaligned(storage_t *storage, void *buf, void *scratch,
                            uint64_t offset, uint32_t bytes)
{
  return storage->ops->write_unaligned
    (storage->ops_data, buf, scratch, offset, bytes);
}

/* generic implementation of unaligned read on top of aligned read */
int read_unaligned_helper(storage_t *storage, void *data,
                          void *buf, void *scratch,
                          uint64_t offset, uint32_t bytes)
{
  const int sector_size = 1 << storage->alignment;

  unsigned unaligned0 = offset % sector_size;
  uint32_t size0 = sector_size - unaligned0;

  /* read unaligned beginning */
  if (unaligned0) {
    if (size0 > bytes) size0 = bytes;
    storage_read(storage, scratch, offset, sector_size);
    memcpy(buf, scratch + unaligned0, size0);

    buf += size0;
    offset += size0;
    bytes -= size0;
  }

  /* read middle */
  assert(IS_ALIGNED(offset, storage->alignment));
  uint32_t size1 = ALIGNED(bytes, storage->alignment);
  if (size1) {
    storage_read(storage, buf, offset, size1);

    buf += size1;
    offset += size1;
    bytes -= size1;
  }

  /* read unaligned end */
  if (bytes) {
    storage_read(storage, scratch, offset, sector_size);
    memcpy(buf, scratch, bytes);
  }

  return 0;
}

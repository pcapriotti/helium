#include "core/storage.h"
#include "core/util.h"

#include <assert.h>
#include <string.h>
#include <math.h>

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
int storage_read_unaligned_helper(storage_ops_t *ops, void *data,
                                  void *buf, void *scratch,
                                  uint64_t offset, uint32_t bytes)
{
  unsigned unaligned0 = MOD64(offset, ops->sector_size);
  uint32_t size0 = ops->sector_size - unaligned0;

  /* read unaligned beginning */
  if (unaligned0) {
    if (size0 > bytes) size0 = bytes;
    if (ops->read(data, scratch,
                  ALIGN64(offset, ops->sector_size),
                  ops->sector_size) == 1)
      return -1;
    memcpy(buf, scratch + unaligned0, size0);

    buf += size0;
    offset += size0;
    bytes -= size0;
  }

  /* read middle */
  assert(ALIGNED64(offset, ops->sector_size));
  uint32_t size1 = ALIGN(bytes, ops->sector_size);
  if (size1) {
    if (ops->read(data, buf, offset, size1) == -1)
      return -1;

    buf += size1;
    offset += size1;
    bytes -= size1;
  }

  /* read unaligned end */
  if (bytes) {
    assert(bytes < (unsigned) ops->sector_size);
    if (ops->read(data, scratch, offset, ops->sector_size) == -1)
      return -1;
    memcpy(buf, scratch, bytes);
  }

  return 0;
}

/* generic implementation of unaligned write on top of aligned read
   and write
*/
int storage_write_unaligned_helper(storage_ops_t *ops, void *data,
                                   void *buf, void *scratch,
                                   uint64_t offset, uint32_t bytes)
{
  unsigned unaligned0 = MOD64(offset, ops->sector_size);
  uint32_t size0 = ops->sector_size - unaligned0;

  /* write unaligned beginning */
  if (unaligned0) {
    if (size0 > bytes) size0 = bytes;
    if (ops->read(data, scratch,
                  ALIGN64(offset, ops->sector_size),
                  ops->sector_size) == -1)
      return -1;
    memcpy(scratch + unaligned0, buf, size0);
    if (ops->write(data, scratch,
                   ALIGN64(offset, ops->sector_size),
                   ops->sector_size) == -1)
      return -1;

    buf += size0;
    offset += size0;
    offset = ALIGN64(offset, ops->sector_size);
    bytes -= size0;
  }

  /* write middle */
  assert(ALIGNED64(offset, ops->sector_size));
  uint32_t size1 = ALIGN(bytes, ops->sector_size);
  if (size1) {
    if (ops->write(data, buf, offset, size1) == -1)
      return -1;

    buf += size1;
    offset += size1;
    bytes -= size1;
  }

  /* write unaligned end */
  if (bytes) {
    assert(ALIGNED64(offset, ops->sector_size));
    assert(bytes < (unsigned) ops->sector_size);
    if (ops->read(data, scratch, offset, ops->sector_size) == -1)
      return -1;
    memcpy(scratch, buf, bytes);
    if (ops->write(data, scratch, offset, ops->sector_size) == -1)
      return -1;
  }

  return 0;
}

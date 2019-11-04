#include "core/storage.h"

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

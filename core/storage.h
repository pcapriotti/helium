#ifndef STORAGE_STORAGE_H
#define STORAGE_STORAGE_H

#include <stdint.h>
#include <stddef.h>

typedef uint64_t storage_offset_t;

/* Storage operations, implemented by e.g. disk drivers.

   All offsets and sizes are in bytes, even for aligned
   operations. Aligned operations expect their arguments to be
   multiples of the sector size.

   Unaligned operations take a scratch buffer, at least the size of a
   sector. This is needed to perform the unaligned access. Unaligned
   operations can be much slower, so the aligned API should be used
   whenever possible. The scratch buffer should not be used when an
   aligned access is performed using the unaligned API.
*/
typedef struct storage_ops {
  int (*read)(void *data, void *buf,
              storage_offset_t offset,
              uint32_t bytes);
  int (*read_unaligned)(void *data, void *buf,
                        void *scratch,
                        storage_offset_t offset,
                        uint32_t bytes);
  int (*write)(void *data, void *buf,
               storage_offset_t offset,
               uint32_t bytes);
  int (*write_unaligned)(void *data, void *buf,
                         void *scratch,
                         storage_offset_t offset,
                         uint32_t bytes);

  size_t sector_size;
} storage_ops_t;

/* storage abstraction */
typedef struct storage {
  /* operations */
  storage_ops_t *ops;
  void *ops_data;
} storage_t;

int storage_read(storage_t *storage, void *buf,
                 storage_offset_t offset, uint32_t bytes);

int storage_read_unaligned(storage_t *storage, void *buf, void *scratch,
                           storage_offset_t offset, uint32_t bytes);

int storage_write(storage_t *storage, void *buf,
                  storage_offset_t offset, uint32_t bytes);

int storage_write_unaligned(storage_t *storage, void *buf, void *scratch,
                            storage_offset_t offset, uint32_t bytes);

int storage_read_unaligned_helper(storage_ops_t *ops, void *data,
                                  void *buf, void *scratch,
                                  storage_offset_t offset, uint32_t bytes);
int storage_write_unaligned_helper(storage_ops_t *storage, void *data,
                                   void *buf, void *scratch,
                                   storage_offset_t offset, uint32_t bytes);

static inline size_t storage_sector_size(storage_t *storage) {
  return storage->ops->sector_size;
}

#endif /* STORAGE_STORAGE_H */

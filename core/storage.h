#ifndef STORAGE_STORAGE_H
#define STORAGE_STORAGE_H

#include <stdint.h>

/* Storage operations, implemented by e.g. disk drivers.

   All offsets and sizes are in bytes, even for aligned
   operations. Aligned operations expect their arguments to be
   multiples of the sector size (i.e. `1 << alignment`).

   Unaligned operations take a scratch buffer, at least the size of a
   sector. This is needed to perform the unaligned access. Unaligned
   operations can be much slower, so the aligned API should be used
   whenever possible. The scratch buffer should not be used when an
   aligned access is performed using the unaligned API.
*/
typedef struct storage_ops {
  int (*read)(void *data, void *buf,
              uint64_t offset,
              uint32_t bytes);
  int (*read_unaligned)(void *data, void *buf,
                        void *scratch,
                        uint64_t offset,
                        uint32_t bytes);
  int (*write)(void *data, void *buf,
               uint64_t offset,
               uint32_t bytes);
  int (*write_unaligned)(void *data, void *buf,
                         void *scratch,
                         uint64_t offset,
                         uint32_t bytes);
} storage_ops_t;

/* storage abstraction */
typedef struct storage {
  /* operations */
  storage_ops_t *ops;
  void *ops_data;

  /* alignment for all storage operations */
  int alignment;
} storage_t;

int storage_read(storage_t *storage, void *buf,
                 uint64_t offset, uint32_t bytes);

int storage_read_unaligned(storage_t *storage, void *buf, void *scratch,
                           uint64_t offset, uint32_t bytes);

int storage_write(storage_t *storage, void *buf,
                  uint64_t offset, uint32_t bytes);

int storage_write_unaligned(storage_t *storage, void *buf, void *scratch,
                            uint64_t offset, uint32_t bytes);

int read_unaligned_helper(storage_t *storage, void *data,
                          void *buf, void *scratch,
                          uint64_t offset, uint32_t bytes);

#endif /* STORAGE_STORAGE_H */

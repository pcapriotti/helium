#include "ext2_fs.h"
#include "ext2.h"

#include "core/debug.h"
#include "core/vfs.h"

#include <stddef.h>
#include <string.h>

typedef struct {
  inode_iterator_t *it;
  size_t offset;
  uint8_t *buffer;
  int dirty; /* whether the buffer needs to be read */
} ext2_vfs_data_t;

static int read(void *data, void *buf, size_t size)
{
  ext2_vfs_data_t *edata = data;

  void *dest = buf;
  const size_t bs = ext2_fs_block_size(edata->it->fs);
  while (size > 0 && !ext2_inode_iterator_end(edata->it)) {
    if (edata->dirty) {
      ext2_inode_iterator_read_into(edata->it, edata->buffer);
      edata->dirty = 0;
    }
    size_t loc_offset = edata->offset % bs;
    size_t loc_size = bs - loc_offset;
    if (loc_size > size) loc_size = size;

    memcpy(dest, edata->buffer + loc_offset, loc_size);

    dest += loc_size;
    size -= loc_size;
    edata->offset += loc_size;
    if (edata->offset % bs == 0) {
      ext2_inode_iterator_next(edata->it);
      edata->dirty = 1;
    }
  }

  return dest - buf;
}

static int move(void *data, size_t offset)
{
  ext2_vfs_data_t *edata = data;
  const size_t bs = ext2_fs_block_size(edata->it->fs);
  int index = ext2_inode_iterator_index(edata->it);
  edata->offset = offset;
  ext2_inode_iterator_set_index(edata->it, offset / bs);
  edata->dirty = edata->dirty ||
    (index != ext2_inode_iterator_index(edata->it));
  return 0;
}

static size_t position(void *data)
{
  ext2_vfs_data_t *edata = data;
  return edata->offset;
}

vfs_file_t *ext2_vfs_file_new(fs_t *fs, ext2_inode_t *inode)
{
  ext2_vfs_data_t *data = MALLOC(sizeof(ext2_vfs_data_t));
  data->it = ext2_inode_iterator_new(fs, inode);
  data->offset = 0;
  data->buffer = MALLOC(ext2_fs_block_size(fs));
  data->dirty = 1;

  vfs_file_t *file = MALLOC(sizeof(vfs_file_t));
  file->data = data;
  file->read = read;
  file->move = move;
  file->position = position;

  return file;
}

void ext2_vfs_file_del(vfs_file_t *file)
{
  ext2_vfs_data_t *data = file->data;
  FREE(data->buffer);
  ext2_inode_iterator_del(data->it);
  FREE(data);
  FREE(file);
}

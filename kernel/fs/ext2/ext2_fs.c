#include "ext2_fs.h"
#include "ext2.h"

#include "core/allocator.h"
#include "core/debug.h"
#include "core/storage.h"
#include "core/vfs.h"

#include <stddef.h>
#include <string.h>

typedef struct {
  ext2_inode_iterator_t *it;
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

static vfs_file_t *create(void *data, const char *path)
{
  ext2_t *fs = data;
  if (!fs) return 0;

  ext2_inode_t *inode = ext2_create(fs, path);
  if (!inode) return 0;

  return ext2_vfs_file_new(fs, inode);
}

static void ext2_vfs_file_del(void *_data)
{
  ext2_vfs_data_t *data = _data;
  allocator_t *allocator = data->it->fs->allocator;
  allocator_free(allocator, data->buffer);
  ext2_inode_iterator_del(data->it);
  allocator_free(allocator, data);
}

vfs_file_t *ext2_vfs_file_new(ext2_t *fs, ext2_inode_t *inode)
{
  ext2_vfs_data_t *data = allocator_alloc(fs->allocator, sizeof(ext2_vfs_data_t));
  data->it = ext2_inode_iterator_new(fs, inode);
  data->offset = 0;
  data->buffer = allocator_alloc(fs->allocator, ext2_fs_block_size(fs));
  data->dirty = 1;

  vfs_file_t *file = allocator_alloc(fs->allocator, sizeof(vfs_file_t));
  file->ops = &ext2_vfs_ops;
  file->data = data;

  return file;
}

vfs_file_t *ext2_vfs_open(void *data, const char *path)
{
  ext2_t *fs = data;
  if (!fs) return 0;

  ext2_inode_t *inode = ext2_get_path_inode(fs, path);
  if (!inode) return 0;

  return ext2_vfs_file_new(fs, inode);
}

int ext2_vfs_close(void *data, vfs_file_t *file)
{
  ext2_t *fs = data;
  ext2_vfs_file_del(file->data);
  allocator_free(fs->allocator, file);
  return 0;
}

vfs_t *ext2_into_vfs(ext2_t *fs)
{
  vfs_t *vfs = allocator_alloc(fs->allocator, sizeof(vfs_t));
  vfs->data = fs;
  vfs->ops = &ext2_vfs_ops;
  return vfs;
}

vfs_t *ext2_vfs_new(storage_t *storage, allocator_t *allocator)
{
  ext2_t *fs = ext2_new_fs(storage, allocator);
  if (!fs) return 0;
  return ext2_into_vfs(fs);
}

void ext2_vfs_del(vfs_t *vfs)
{
  ext2_t *fs = vfs->data;
  allocator_t *allocator = fs->allocator;
  ext2_free_fs(fs);
  allocator_free(allocator, vfs);
}

vfs_ops_t ext2_vfs_ops = {
  .open = ext2_vfs_open,
  .close = ext2_vfs_close,
  .new = ext2_vfs_new,
  .del = ext2_vfs_del,
  .create = create,
  .read = read,
  .move = move,
  .position = position,
};


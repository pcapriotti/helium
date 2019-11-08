#include "core/allocator.h"
#include "core/debug.h"
#include "core/storage.h"
#include "core/vfs.h"
#include "fs/fat/fat_vfs.h"
#include "fs/fat/fat.h"

vfs_t *fat_vfs_new(storage_t *storage, allocator_t *allocator)
{
  fat_t *fs = allocator_alloc(allocator, sizeof(fat_t));
  fat_init(fs, storage, allocator);
  vfs_t *vfs = allocator_alloc(allocator, sizeof(vfs_t));
  vfs->data = fs;
  vfs->ops = &fat_vfs_ops;
  return vfs;
}

void fat_vfs_del(vfs_t *vfs)
{
  fat_t *fs = vfs->data;
  allocator_t *allocator = fs->allocator;
  fat_cleanup(fs);
  allocator_free(allocator, fs);
  allocator_free(allocator, vfs);
}

int fat_vfs_read(void *data, void *buf, size_t size)
{
  return -1;
}

int fat_vfs_move(void *data, size_t offset)
{
  return -1;
}

size_t fat_vfs_position(void *data)
{
  return 0;
}

vfs_file_t *fat_vfs_open(void *data, const char *path)
{
  fat_t *fs = data;

  serial_printf("opening %s\n", path);

  unsigned cluster;
  if (fat_path_cluster(fs, path, &cluster) == -1)
    return 0;

  vfs_file_t *file = allocator_alloc(fs->allocator, sizeof(vfs_file_t));
  file->data = (void *) cluster;
  file->read = fat_vfs_read;
  file->move = fat_vfs_move;
  file->position = fat_vfs_position;

  return file;
}

int fat_vfs_close(void *data, vfs_file_t *file)
{
  fat_t *fs = data;
  allocator_free(fs->allocator, file);
  return 0;
}

vfs_ops_t fat_vfs_ops = {
  .new = fat_vfs_new,
  .del = fat_vfs_del,
  .open = fat_vfs_open,
  .close = fat_vfs_close,
};

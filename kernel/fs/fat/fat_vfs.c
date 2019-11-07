#include "core/allocator.h"
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

vfs_file_t *fat_vfs_open(void *data, const char *path)
{
  return 0;
}

int fat_vfs_close(void *data, vfs_file_t *file)
{
  return 0;
}

vfs_ops_t fat_vfs_ops = {
  .new = fat_vfs_new,
  .del = fat_vfs_del,
  .open = fat_vfs_open,
  .close = fat_vfs_close,
};

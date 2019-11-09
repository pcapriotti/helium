#include "core/allocator.h"
#include "core/debug.h"
#include "core/storage.h"
#include "core/util.h"
#include "core/vfs.h"
#include "fs/fat/fat_vfs.h"
#include "fs/fat/fat.h"

#include <assert.h>
#include <string.h>

typedef struct fat_vfs_file {
  fat_t *fs;
  unsigned cluster;
  int cluster_index;
  unsigned cluster0;
  size_t offset;
  vfs_file_t vfs_file;
  void *buffer;
  int dirty;
} fat_vfs_file_t;

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

static void fat_vfs_file_adjust(fat_vfs_file_t *file)
{
  while (file->offset >= file->fs->cluster_size) {
    if (fat_end_of_chain(file->fs, file->cluster)) break;
    file->cluster = fat_map_next(file->fs, file->cluster);
    file->cluster_index++;
    file->offset -= file->fs->cluster_size;
    file->dirty = 1;
#if FAT_DEBUG
    serial_printf("[fat] adjust, offset: %u, cluster: %u, index: %u\n",
                  file->offset, file->cluster, file->cluster_index);
#endif
  }
}

static int fat_vfs_read_small(fat_vfs_file_t *file, void *buf, size_t size)
{
#if FAT_DEBUG
  serial_printf("[fat] reading %u bytes at cluster %u offset %u\n",
                size, file->cluster, file->offset);
#endif
  if (fat_end_of_chain(file->fs, file->cluster))
    return 0;

  if (file->offset + size > file->fs->cluster_size) {
    size = file->fs->cluster_size - file->offset;
  }

  if (!file->buffer) {
    file->buffer = allocator_alloc(file->fs->allocator,
                                   file->fs->cluster_size);
  }

  if (file->dirty) {
    fat_read_cluster(file->fs, file->buffer, file->cluster);
    file->dirty = 0;
  }

  memcpy(buf, file->buffer + file->offset, size);
  file->offset += size;
  fat_vfs_file_adjust(file);

  return size;
}

int fat_vfs_read(void *data, void *buf, size_t size)
{
  fat_vfs_file_t *file = data;

  int total = 0;
  while (size > 0) {
    int len = fat_vfs_read_small(file, buf, size);
    total += len;
    if (len == 0) return total;
    size -= len;
    buf += len;
  }

  return total;
}

int fat_vfs_move(void *data, size_t offset)
{
  fat_vfs_file_t *file = data;
#if FAT_DEBUG
  serial_printf("[fat] moving from cluster %u offset %u to abs offset %u\n",
                file->cluster, file->offset, offset);
#endif

  size_t base = file->cluster_index * file->fs->cluster_size;

  if (offset < base) {
    file->cluster = file->cluster0;
    file->cluster_index = 0;
    file->offset = offset;
  }
  else {
    file->offset = offset - base;
  }

  fat_vfs_file_adjust(file);
  return 0;
}

size_t fat_vfs_position(void *data)
{
  fat_vfs_file_t *file = data;
  return file->offset;
}

vfs_file_t *fat_vfs_open(void *data, const char *path)
{
  fat_t *fs = data;

  unsigned cluster;
  if (fat_path_cluster(fs, path, &cluster) == -1)
    return 0;
  assert(cluster >= 2);

  fat_vfs_file_t *file = allocator_alloc(fs->allocator, sizeof(fat_vfs_file_t));
  file->vfs_file.data = file;
  file->vfs_file.read = fat_vfs_read;
  file->vfs_file.move = fat_vfs_move;
  file->vfs_file.position = fat_vfs_position;
  file->fs = fs;
  file->cluster0 = cluster;
  file->cluster = cluster;
  file->cluster_index = 0;
  file->offset = 0;
  file->buffer = 0;
  file->dirty = 1;

  return &file->vfs_file;
}

int fat_vfs_close(void *data, vfs_file_t *file)
{
  fat_t *fs = data;
  allocator_free(fs->allocator, file->data);
  return 0;
}

vfs_ops_t fat_vfs_ops = {
  .new = fat_vfs_new,
  .del = fat_vfs_del,
  .open = fat_vfs_open,
  .close = fat_vfs_close,
};

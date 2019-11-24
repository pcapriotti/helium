#include "allocator.h"
#include "storage.h"
#include "vfs.h"

int vfs_read(vfs_file_t *file, void *buf, size_t size)
{
  return file->ops->read(file->data, buf, size);
}

int vfs_move(vfs_file_t *file, size_t offset)
{
  return file->ops->move(file->data, offset);
}

size_t vfs_position(vfs_file_t *file)
{
  return file->ops->position(file->data);
}

int vfs_resize(vfs_file_t *file, uint64_t size)
{
  return file->ops->resize(file->data, size);
}

int vfs_move_rel(vfs_file_t *file, size_t offset)
{
  return vfs_move(file, offset + vfs_position(file));
}

vfs_file_t *vfs_open(vfs_t *fs, const char *path)
{
  return fs->ops->open(fs->data, path);
}

vfs_file_t *vfs_create(vfs_t *fs, const char *path)
{
  return fs->ops->create(fs->data, path);
}

int vfs_close(vfs_t *fs, vfs_file_t *file)
{
  return fs->ops->close(fs->data, file);
}

vfs_t *vfs_new(vfs_ops_t *ops, storage_t *s, allocator_t *a)
{
  return ops->new(s, a);
}

void vfs_del(vfs_t *fs)
{
  fs->ops->del(fs);
}

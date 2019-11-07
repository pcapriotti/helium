#include "vfs.h"

int vfs_read(vfs_file_t *file, void *buf, size_t size)
{
  return file->read(file->data, buf, size);
}

int vfs_move(vfs_file_t *file, size_t offset)
{
  return file->move(file->data, offset);
}

size_t vfs_position(vfs_file_t *file)
{
  return file->position(file->data);
}

int vfs_move_rel(vfs_file_t *file, size_t offset)
{
  return vfs_move(file, offset + vfs_position(file));
}

vfs_file_t *vfs_open(vfs_t *fs, const char *path)
{
  return fs->open(fs->data, path);
}

int vfs_close(vfs_t *fs, vfs_file_t *file)
{
  return fs->close(fs->data, file);
}

void vfs_del(vfs_t *fs)
{
  fs->del(fs);
}

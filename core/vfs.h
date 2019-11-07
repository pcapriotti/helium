#ifndef VFS_H
#define VFS_H

#include <stddef.h>
#include <stdint.h>

typedef struct vfs_file {
  void *data;
  int (*read)(void *data, void *buf, size_t size);
  int (*move)(void *data, size_t offset);
  size_t (*position)(void *data);
} vfs_file_t;

typedef struct vfs {
  void *data;
  vfs_file_t *(*open)(void *data, const char *path);
  int (*close)(void *data, vfs_file_t *file);
  void (*del)(struct vfs *vfs);
} vfs_t;

vfs_file_t *vfs_open(vfs_t *fs, const char *path);
int vfs_close(vfs_t *fs, vfs_file_t *file);
void vfs_del(vfs_t *vfs);

int vfs_read(vfs_file_t *file, void *buf, size_t size);
int vfs_move(vfs_file_t *file, size_t offset);
size_t vfs_position(vfs_file_t *file);
int vfs_move_rel(vfs_file_t *file, size_t offset);

#endif /* VFS_H */

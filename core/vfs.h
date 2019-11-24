#ifndef VFS_H
#define VFS_H

#include <stddef.h>
#include <stdint.h>

struct storage;
struct allocator;

struct vfs;

typedef struct vfs_ops {
  struct vfs_file *(*open)(void *data, const char *path);
  int (*close)(void *data, struct vfs_file *file);
  struct vfs_file *(*create)(void *data, const char *path);

  struct vfs *(*new)(struct storage *storage,
                     struct allocator *allocator);
  void (*del)(struct vfs *vfs);
  int (*read)(void *data, void *buf, size_t size);
  int (*move)(void *data, size_t offset);
  size_t (*position)(void *data);
  int (*resize)(void *data, size_t size);
} vfs_ops_t;

typedef struct vfs_file {
  void *data;
  vfs_ops_t *ops;
} vfs_file_t;

typedef struct vfs {
  void *data;
  vfs_ops_t *ops;
} vfs_t;

vfs_file_t *vfs_open(vfs_t *fs, const char *path);
int vfs_close(vfs_t *fs, vfs_file_t *file);
void vfs_del(vfs_t *vfs);

vfs_file_t *vfs_create(vfs_t *fs, const char *path);
int vfs_read(vfs_file_t *file, void *buf, size_t size);
int vfs_move(vfs_file_t *file, size_t offset);
size_t vfs_position(vfs_file_t *file);
int vfs_move_rel(vfs_file_t *file, size_t offset);

#endif /* VFS_H */

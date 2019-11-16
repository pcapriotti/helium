#include <error.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "core/allocator.h"
#include "core/storage.h"
#include "kernel/fs/ext2/ext2.h"

int test_read(void *data, void *buf,
              uint64_t offset, uint32_t bytes)
{
  FILE *image = (FILE*)data;
  fseek(image, offset, SEEK_SET);
  fread(buf, bytes, 1, image);
  return 0;
}

int test_read_unaligned(void *data, void *buf, void *scratch,
                        uint64_t offset, uint32_t bytes)
{
  return test_read(data, buf, offset, bytes);
}

storage_ops_t test_storage_ops = {
  .read_unaligned = test_read_unaligned,
  .read = test_read,
  .write_unaligned = 0,
  .write = 0,
  .alignment = 0,
};

storage_t test_storage = {
  .ops = &test_storage_ops,
  .ops_data = 0,
};

void *stdlib_allocator_alloc(void *data, size_t size)
{
  return malloc(size);
}

void stdlib_allocator_free(void *data, void *x)
{
  free(x);
}

allocator_t stdlib_allocator = {
  .alloc = stdlib_allocator_alloc,
  .free = stdlib_allocator_free,
};

int main(int argc, char **argv)
{
  if (argc < 2) {
    error(2, 0, "Usage: %s IMAGE", argv[0]);
    exit(1);
  }
  FILE *image = fopen(argv[1], "rb");
  if (!image) {
    error(1, errno, "Could not open %s", argv[1]);
  }

  test_storage.ops_data = image;
  ext2_t *fs = ext2_new_fs(&test_storage, &stdlib_allocator);
  if (!fs) {
    error(1, 0, "Invalid superblock");
  }

  /* read example file */
  if (0) {
    ext2_inode_t *inode = ext2_get_path_inode(fs, "drivers/keyboard/keyboard.c");
    if (inode) {
      ext2_inode_t tmp = *inode;
      ext2_inode_iterator_t *it = ext2_inode_iterator_new(fs, &tmp);
      while (!ext2_inode_iterator_end(it)) {
        char *buf = ext2_inode_iterator_read(it);
        write(1, buf, ext2_inode_iterator_block_size(it));
        ext2_inode_iterator_next(it);
      }
      free(it);
    }
  }

  /* create new file */
  int index = ext2_get_free_inode(fs, 0);
  printf("index = %d\n", index);

  ext2_free_fs(fs);
}

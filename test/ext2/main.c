#include <error.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "core/allocator.h"
#include "core/storage.h"
#include "kernel/fs/ext2/ext2.h"
#include "kernel/fs/ext2/iterator.h"

int test_read(void *data, void *buf,
              uint64_t offset, uint32_t bytes)
{
  FILE *image = (FILE*)data;
  printf("reading %u bytes at offset %lu\n", bytes, offset);
  fseek(image, offset, SEEK_SET);
  fread(buf, bytes, 1, image);
  return 0;
}

int test_read_unaligned(void *data, void *buf, void *scratch,
                        uint64_t offset, uint32_t bytes)
{
  return test_read(data, buf, offset, bytes);
}

int test_write(void *data, void *buf,
               uint64_t offset, uint32_t bytes)
{
  printf("writing %u bytes at offset %#lx\n", bytes, offset);
  for (int i = 0; i < (int) bytes; i++) {
    printf(" %02x", ((unsigned char*) buf)[i]);
  }
  printf("\n");

  FILE *image = (FILE*)data;
  fseek(image, offset, SEEK_SET);
  fwrite(buf, bytes, 1, image);


  /* check what we wrote */
  {
    unsigned char *test = malloc(10);
    fseek(image, offset, SEEK_SET);
    fread(test, 10, 1, image);
    printf("written: ");
    for (int i = 0; i < 10; i++) {
      printf(" %02x", test[i]);
    }
    printf("\n");
    free(test);
  }

  return 0;
}

int test_write_unaligned(void *data, void *buf, void *scratch,
                         uint64_t offset, uint32_t bytes)
{
  return test_write(data, buf, offset, bytes);
}

storage_ops_t test_storage_ops = {
  .read_unaligned = test_read_unaligned,
  .read = test_read,
  .write_unaligned = test_write_unaligned,
  .write = test_write,
  .sector_size = 32,
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
  FILE *image = fopen(argv[1], "rb+");
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
  ext2_inode_t *inode = ext2_create(fs, "/test");

  ext2_free_fs(fs);
  fclose(image);
}

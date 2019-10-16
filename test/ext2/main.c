#include <error.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../kernel/fs/ext2/ext2.h"

void test_read(void *data, void *buf,
               unsigned int offset, unsigned int bytes)
{
  FILE *image = (FILE*)data;
  fseek(image, offset, SEEK_SET);
  fread(buf, bytes, 1, image);
}

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

  fs_t *fs = ext2_new_fs(test_read, image);
  if (!fs) {
    error(1, 0, "Invalid superblock");
  }

  /* read example file */
  inode_t *inode = ext2_get_path_inode(fs, "kernel/drivers/keyboard.c");
  if (inode) {
    inode_t tmp = *inode;
    inode_iterator_t *it = ext2_inode_iterator_new(fs, &tmp);
    while (!ext2_inode_iterator_end(it)) {
      char *buf = ext2_inode_iterator_read(it);
      write(1, buf, ext2_inode_iterator_block_size(it));
      ext2_inode_iterator_next(it);
    }
    free(it);
  }

  ext2_free_fs(fs);
}

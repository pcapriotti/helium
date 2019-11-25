#ifndef FS_EXT2_DIR_H
#define FS_EXT2_DIR_H

typedef struct ext2_dir_iterator_t {
  ext2_t *fs;
  uint32_t index;
  ext2_inode_iterator_t inode_it;
  unsigned block_index;
  void *block;
  size_t block_offset;
} ext2_dir_iterator_t;


int ext2_dir_iterator_init(ext2_dir_iterator_t *it,
                           ext2_t *fs, ext2_inode_t *inode);

void ext2_dir_iterator_cleanup(ext2_dir_iterator_t *it);

void ext2_dir_iterator_write(ext2_dir_iterator_t *it, ext2_dir_entry_t *entry);

ext2_dir_entry_t *ext2_dir_iterator_next(ext2_dir_iterator_t *it);

#endif /* FS_EXT2_DIR_H */

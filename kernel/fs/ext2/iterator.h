#ifndef FS_EXT2_ITERATOR_H
#define FS_EXT2_ITERATOR_H

/* inode block iterator API */
typedef struct {
  ext2_t *fs;
  ext2_inode_t inode;
  uint32_t index;
} ext2_inode_iterator_t;

void ext2_inode_iterator_init(ext2_inode_iterator_t *it, ext2_t *fs, ext2_inode_t *inode);
ext2_inode_iterator_t *ext2_inode_iterator_new(ext2_t *fs, ext2_inode_t *inode);
int ext2_inode_iterator_resize(ext2_inode_iterator_t *it, uint64_t size);
void ext2_inode_iterator_del(ext2_inode_iterator_t *it);
uint32_t ext2_inode_iterator_datablock(ext2_inode_iterator_t *it);
void *ext2_inode_iterator_read(ext2_inode_iterator_t *it);
void ext2_inode_iterator_read_into(ext2_inode_iterator_t *it, void *buffer);
void ext2_inode_iterator_next(ext2_inode_iterator_t *it);
void ext2_inode_iterator_set_index(ext2_inode_iterator_t *it, int index);
int ext2_inode_iterator_index(ext2_inode_iterator_t *it);
uint32_t ext2_inode_iterator_block_size(ext2_inode_iterator_t *it);
int ext2_inode_iterator_end(ext2_inode_iterator_t *it);

#endif /* FS_EXT2_ITERATOR_H */

#include "core/allocator.h"
#include "fs/ext2/ext2.h"
#include "fs/ext2/iterator.h"

void ext2_inode_iterator_init(ext2_inode_iterator_t *it,
                              ext2_t *fs, ext2_inode_t *inode)
{
  it->fs = fs;
  it->inode = *inode;
  it->index = 0;
}

ext2_inode_iterator_t *ext2_inode_iterator_new(ext2_t *fs,
                                               ext2_inode_t *inode)
{
  ext2_inode_iterator_t *it =
    allocator_alloc(fs->allocator,
                    sizeof(ext2_inode_iterator_t));
  ext2_inode_iterator_init(it, fs, inode);
  return it;
}

void ext2_inode_iterator_del(ext2_inode_iterator_t *it)
{
  allocator_free(it->fs->allocator, it);
}

uint32_t ext2_inode_iterator_datablock(ext2_inode_iterator_t *it) {
  return *ext2_inode_block_pointer(it->fs, &it->inode, it->index);
}

void *ext2_inode_iterator_read(ext2_inode_iterator_t *it)
{
  uint32_t block = ext2_inode_iterator_datablock(it);
  return ext2_read_block(it->fs, block);
}

/* read into a preallocated buffer, which must be at least as big as
the block size */
void ext2_inode_iterator_read_into(ext2_inode_iterator_t *it, void *buffer)
{
  uint32_t block = ext2_inode_iterator_datablock(it);
  return ext2_read_block_into(it->fs, block, buffer);
}

int ext2_inode_iterator_index(ext2_inode_iterator_t *it)
{
  return it->index;
}

void ext2_inode_iterator_set_index(ext2_inode_iterator_t *it, int index)
{
  it->index = index;
}

void ext2_inode_iterator_next(ext2_inode_iterator_t *it)
{
  it->index++;
}

uint32_t ext2_inode_iterator_block_size(ext2_inode_iterator_t *it) {
  uint64_t offset = (uint64_t) it->index * (uint64_t) it->fs->block_size;
  uint64_t ret = ext2_inode_size(&it->inode) - offset;
  if (ret > it->fs->block_size) {
    return it->fs->block_size;
  } else {
    return ret;
  }
}

int ext2_inode_iterator_end(ext2_inode_iterator_t *it) {
  return it->index * it->fs->block_size >= it->inode.size_lo;
}

int ext2_inode_iterator_resize(ext2_inode_iterator_t *it, uint64_t size)
{
  return ext2_inode_resize(it->fs, &it->inode, size);
}

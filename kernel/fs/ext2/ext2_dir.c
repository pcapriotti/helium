#include "core/allocator.h"
#include "fs/ext2/ext2.h"
#include "fs/ext2/ext2_dir.h"

int ext2_dir_iterator_init(ext2_dir_iterator_t *it,
                           ext2_t *fs, ext2_inode_t *inode)
{
  if ((inode->type & 0xf000) != INODE_TYPE_DIRECTORY) {
    return -1;
  }

  it->fs = fs;
  it->index = 0;
  it->block_index = 0;
  it->block = 0;
  it->block_offset = 0;
  ext2_inode_iterator_init(&it->inode_it, fs, inode);

  return 0;
}

void ext2_dir_iterator_cleanup(ext2_dir_iterator_t *it)
{
  allocator_free(it->fs->allocator, it->block);
}

void ext2_dir_iterator_write(ext2_dir_iterator_t *it, ext2_dir_entry_t *entry)
{
  ext2_write(it->fs, it->block_index, it->block,
             entry, sizeof(ext2_dir_entry_t));
}

ext2_dir_entry_t *ext2_dir_iterator_next(ext2_dir_iterator_t *it)
{
  if (!it->block || it->block_offset >= it->fs->block_size) {
    if (!it->block) it->block = allocator_alloc
                      (it->fs->allocator, it->fs->block_size);
    if (ext2_inode_iterator_end(&it->inode_it)) return 0;
    it->block_index = ext2_inode_iterator_datablock(&it->inode_it);
    ext2_read_block_into(it->fs, it->block_index, it->block);
  }

  if (it->block_offset + sizeof(ext2_dir_entry_t) >= it->fs->block_size) {
    /* directory entries cannot span multiple blocks */
    return 0;
  }

  ext2_dir_entry_t *entry = it->block + it->block_offset;
  if (entry->size < sizeof(ext2_dir_entry_t)) {
    /* entry is too small */
    return 0;
  }

  if (it->block_offset + entry->size > it->fs->block_size) {
    /* directory entries cannot span multiple blocks */
    return 0;
  }

  if (entry->name_length_lo + sizeof(ext2_dir_entry_t) > entry->size) {
    /* entry name is too long */
    return 0;
  }

  it->block_offset += entry->size;
  it->index++;

  return entry;
}

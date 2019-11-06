#include "fs/ext2/ext2.h"
#include "core/allocator.h"
#include "core/storage.h"

#include <stddef.h>

#define allocator_alloc allocator_alloc
#define FREE allocator_free

#if _HELIUM
# include "core/debug.h"
# define TRACE serial_printf
#else
# include <stdio.h>
# define TRACE printf
#endif /* _HELIUM */

#include <stddef.h>
#include <string.h>

#define EXT2_DEBUG 1

#define ROUND_UP(a, b) (((a) + (b) - 1) / (b))

void ext2_read_block_into(ext2_t *fs, unsigned int offset, void *buffer)
{
  storage_read(fs->storage,
               buffer,
               fs->block_size * offset,
               fs->block_size);
}

void* ext2_read_block(ext2_t *fs, unsigned int offset)
{
  ext2_read_block_into(fs, offset, fs->buf);
  return fs->buf;
}

size_t ext2_fs_block_size(ext2_t *fs)
{
  return fs->block_size;
}

int ext2_locate_superblock(storage_t *storage, void *scratch, ext2_superblock_t *sb)
{
  storage_read_unaligned(storage, sb, scratch, 1024, sizeof(ext2_superblock_t));
  return (sb->signature == 0xef53);
}

ext2_t *ext2_new_fs(storage_t *storage, allocator_t *allocator)
{
  ext2_superblock_t sb;
#if EXT2_DEBUG
  TRACE("Locating superblock\n");
#endif

  void *scratch = allocator_alloc(allocator, 1 << storage->ops->alignment);

  if (!ext2_locate_superblock(storage, scratch, &sb)) {
#if EXT2_DEBUG
    TRACE("Could not find superblock\n");
#endif
    FREE(allocator, scratch);
    return 0;
  }

  ext2_t *fs = allocator_alloc(allocator, sizeof(ext2_t));
  fs->storage = storage;
  fs->allocator = allocator;
  fs->scratch = scratch;
  fs->block_size = ext2_block_size(&sb);
  fs->inode_size = ext2_inode_size(&sb);
  fs->inodes_per_group = sb.inodes_per_group;
  fs->superblock_offset = sb.superblock_offset;
  fs->buf = allocator_alloc(fs->allocator, fs->block_size);

#if EXT2_DEBUG
  TRACE("block size: %#lx inode size: %#lx superblock offset: %#x\n",
          fs->block_size, fs->inode_size, fs->superblock_offset);
#endif

  return fs;
}

void ext2_free_fs(ext2_t *fs) {
  FREE(fs->allocator, fs->buf);
  FREE(fs->allocator, fs);
}

int ext2_num_bgroups(ext2_superblock_t *sb) {
  if (sb->blocks_per_group == 0) {
    return -1;
  }
  int n1 = ROUND_UP(sb->num_blocks, sb->blocks_per_group);
  if (sb->inodes_per_group == 0) {
    return -2;
  }
  int n2 = ROUND_UP(sb->num_inodes, sb->inodes_per_group);
  if (n1 != n2) {
    return -3;
  }
  return n1;
}

ext2_inode_t *ext2_get_inode(ext2_t* fs, unsigned int index)
{
  index--;
  unsigned int group = index / fs->inodes_per_group;

  /* retrieve bgroup descriptor */
  unsigned int descriptors_per_block = fs->block_size /
    sizeof(ext2_group_descriptor_t);
  unsigned int descriptor_block = group / descriptors_per_block;
  ext2_group_descriptor_t *gdesc_table
    = ext2_read_block(fs, fs->superblock_offset + 1 + descriptor_block);
  ext2_group_descriptor_t *gdesc = &gdesc_table[group % descriptors_per_block];

  unsigned int index_in_group = index % fs->inodes_per_group;
  unsigned int inodes_per_block = fs->block_size / fs->inode_size;
  unsigned int block = index_in_group / inodes_per_block;

  void *table = ext2_read_block(fs, gdesc->inode_table_offset + block);
  unsigned int index_in_block = index_in_group % inodes_per_block;
  return table + fs->inode_size * index_in_block;
}

ext2_inode_t *ext2_get_path_inode(ext2_t *fs, const char *path)
{
  /* get root first */
  ext2_inode_t *inode = ext2_get_inode(fs, 2);

  if (!inode) {
    return 0;
  }

  int path_len = strlen(path);
  char *pbuf = allocator_alloc(fs->allocator, path_len + 1);
  strcpy(pbuf, path);

  char *saveptr = 0;
  char *token = strtok_r(pbuf, "/", &saveptr);
  if (token == 0) { /* root */
    FREE(fs->allocator, pbuf);
    return inode;
  }
  while (token != 0) {
    ext2_inode_t parent = *inode;
    inode = ext2_find_entry(fs, &parent, token);

    if (inode == 0) {
      token = 0;
    } else {
      token = strtok_r(0, "/", &saveptr);
    }
  }

  FREE(fs->allocator, pbuf);
  return inode;
}

ext2_inode_t *ext2_find_entry(ext2_t *fs,
                              ext2_inode_t *inode,
                              const char *name)
{
  uint16_t name_length = strlen(name);

  ext2_dir_iterator_t it;
  if (ext2_dir_iterator_init(&it, fs, inode) == -1)
    return 0;

  ext2_inode_t *ret = 0;
  ext2_dir_entry_t *entry = 0;

  while ((entry = ext2_dir_iterator_next(&it))) {
    if (entry->name_length_lo == name_length &&
        !memcmp(entry->name, name, entry->name_length_lo)) {
      ret = ext2_get_inode(fs, entry->inode);
    }
  }
  ext2_dir_iterator_cleanup(&it);

  return ret;
}

uint32_t ext2_block_size(ext2_superblock_t *sb)
{
  return 1024 << sb->log_block_size;
}

uint16_t ext2_inode_size(ext2_superblock_t *sb)
{
  if (sb->version_major >= 1) {
    return sb->inode_size;
  } else {
    return 128;
  }
}

void ext2_inode_iterator_init(ext2_inode_iterator_t *it,
                              ext2_t *fs, ext2_inode_t *inode)
{
  it->fs = fs;
  it->inode = inode;
  it->index = 0;
}

ext2_inode_iterator_t *ext2_inode_iterator_new(ext2_t *fs,
                                               ext2_inode_t *inode)
{
  ext2_inode_iterator_t *it = allocator_alloc(fs->allocator,
                                     sizeof(ext2_inode_iterator_t));
  ext2_inode_iterator_init(it, fs, inode);
  return it;
}

void ext2_inode_iterator_del(ext2_inode_iterator_t *it)
{
  FREE(it->fs->allocator, it);
}

uint32_t ext2_inode_iterator_datablock(ext2_inode_iterator_t *it) {
  uint32_t index, index1, index2, index3;

  /* level 0 */
  if (it->index < 12) {
    return it->inode->pointer0[it->index];
  }
  uint32_t pointers_per_block = it->fs->block_size / 4;

  /* level 1 */
  index = it->index - 12;
  if (index < pointers_per_block) {
    uint32_t *pointer1 = ext2_read_block(it->fs, it->inode->pointer1);
    return pointer1[index];
  }

  /* level 2 */
  index -= pointers_per_block;
  index2 = index / pointers_per_block;
  index1 = index % pointers_per_block;
  if (index2 < pointers_per_block) {
    uint32_t *pointer2 = ext2_read_block(it->fs, it->inode->pointer2);
    uint32_t *pointer1 = ext2_read_block(it->fs, pointer2[index2]);
    return pointer1[index1];
  }

  /* level 3 */
  index -= pointers_per_block * pointers_per_block;
  index1 = index % pointers_per_block;
  index /= pointers_per_block;
  index2 = index % pointers_per_block;
  index3 = index / pointers_per_block;
  if (index3 < pointers_per_block) {
    uint32_t *pointer3 = ext2_read_block(it->fs, it->inode->pointer3);
    uint32_t *pointer2 = ext2_read_block(it->fs, pointer3[index3]);
    uint32_t *pointer1 = ext2_read_block(it->fs, pointer2[index2]);
    return pointer1[index1];
  }

  return 0;
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
  uint32_t offset = it->index * it->fs->block_size;
  uint32_t ret = it->inode->size_lo - offset;
  if (ret > it->fs->block_size) {
    return it->fs->block_size;
  } else {
    return ret;
  }
}

int ext2_inode_iterator_end(ext2_inode_iterator_t *it) {
  return it->index * it->fs->block_size >= it->inode->size_lo;
}

int ext2_dir_iterator_init(ext2_dir_iterator_t *it,
                           ext2_t *fs, ext2_inode_t *inode)
{
  if ((inode->type & 0xf000) != INODE_TYPE_DIRECTORY) {
    return -1;
  }

  it->fs = fs;
  it->inode = inode;
  it->index = 0;
  it->block = 0;
  it->block_offset = 0;
  ext2_inode_iterator_init(&it->inode_it, fs, inode);

  return 0;
}

void ext2_dir_iterator_cleanup(ext2_dir_iterator_t *it)
{
  FREE(it->fs->allocator, it->block);
}

ext2_dir_entry_t *ext2_dir_iterator_next(ext2_dir_iterator_t *it)
{
  while (1) {
    if (!it->block || it->block_offset >= it->fs->block_size) {
      if (!it->block) it->block = allocator_alloc
                        (it->fs->allocator, it->fs->block_size);
      if (ext2_inode_iterator_end(&it->inode_it)) return 0;
      uint32_t block_num = ext2_inode_iterator_datablock(&it->inode_it);
      ext2_read_block_into(it->fs, block_num, it->block);
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

    if (!entry->inode) {
      /* blank entry */
      continue;
    }

    it->block_offset += entry->size;
    it->index++;

    return entry;
  }

  return 0;
}

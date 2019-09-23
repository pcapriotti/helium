#include "ext2.h"

#include <stddef.h>

#if _HELIUM
# include "core/debug.h"
# define TRACE kprintf
#else
# include <stdio.h>
# include <stdlib.h>
# define MALLOC malloc
# define FREE free
# define TRACE printf
#endif /* _HELIUM */

#include <stddef.h>
#include <string.h>

#define EXT2_DEBUG 1

#define ROUND_UP(a, b) (((a) + (b) - 1) / (b))

struct fs_struct {
  ext2_disk_read_t *read;
  void *read_data;
  unsigned char *buf; /* must be at least as big as the block size */
  size_t block_size;
  size_t inode_size;
  uint32_t inodes_per_group;
  uint32_t superblock_offset;
};

void ext2_read_block_into(fs_t *fs, unsigned int offset, void *buffer)
{
  fs->read(fs->read_data, buffer,
           fs->block_size * offset,
           fs->block_size);
}

void* ext2_read_block(fs_t *fs, unsigned int offset)
{
  ext2_read_block_into(fs, offset, fs->buf);
  return fs->buf;
}

size_t ext2_fs_block_size(fs_t *fs)
{
  return fs->block_size;
}

int ext2_locate_superblock(ext2_disk_read_t *read, void *read_data, superblock_t *sb)
{
  read(read_data, sb, 1024, sizeof(superblock_t));
  return (sb->signature == 0xef53);
}

fs_t *ext2_new_fs(ext2_disk_read_t *read, void *read_data)
{
  superblock_t sb;
#if EXT2_DEBUG
  TRACE("Locating superblock\n");
#endif

  if (!ext2_locate_superblock(read, read_data, &sb)) {
#if EXT2_DEBUG
    TRACE("Could not find superblock\n");
#endif
    return 0;
  }

  fs_t *fs = MALLOC(sizeof(fs_t));
  fs->read = read;
  fs->read_data = read_data;
  fs->block_size = ext2_block_size(&sb);
  fs->inode_size = ext2_inode_size(&sb);
  fs->inodes_per_group = sb.inodes_per_group;
  fs->superblock_offset = sb.superblock_offset;
  fs->buf = MALLOC(fs->block_size);

#if EXT2_DEBUG
  TRACE("block size: %#lx inode size: %#lx superblock offset: %#x\n",
          fs->block_size, fs->inode_size, fs->superblock_offset);
#endif

  return fs;
}

void ext2_free_fs(fs_t *fs) {
  FREE(fs->buf);
  FREE(fs);
}

int ext2_num_bgroups(superblock_t *sb) {
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

inode_t *ext2_get_inode(fs_t* fs, unsigned int index)
{
  index--;
  unsigned int group = index / fs->inodes_per_group;

  /* retrieve bgroup descriptor */
  unsigned int descriptors_per_block = fs->block_size / sizeof(group_descriptor_t);
  unsigned int descriptor_block = group / descriptors_per_block;
  group_descriptor_t *gdesc_table
    = ext2_read_block(fs, fs->superblock_offset + 1 + descriptor_block);
  group_descriptor_t *gdesc = &gdesc_table[group % descriptors_per_block];

  unsigned int index_in_group = index % fs->inodes_per_group;
  unsigned int inodes_per_block = fs->block_size / fs->inode_size;
  unsigned int block = index_in_group / inodes_per_block;

  void *table = ext2_read_block(fs, gdesc->inode_table_offset + block);
  unsigned int index_in_block = index_in_group % inodes_per_block;
  return table + fs->inode_size * index_in_block;
}

inode_t *ext2_get_path_inode(fs_t *fs, const char *path)
{

  /* get root first */
  inode_t *inode = ext2_get_inode(fs, 2);

  if (!inode) {
    return 0;
  }

  int path_len = strlen(path);
  char *pbuf = MALLOC(path_len + 1);
  strcpy(pbuf, path);

  char *saveptr = 0;
  char *token = strtok_r(pbuf, "/", &saveptr);
  if (token == 0) { /* root */
    FREE(pbuf);
    return inode;
  }
  while (token != 0) {
    inode_t parent = *inode;
    inode = ext2_find_entry(fs, &parent, token);

    if (inode == 0) {
      token = 0;
    } else {
      token = strtok_r(0, "/", &saveptr);
    }
  }

  FREE(pbuf);
  return inode;
}

inode_t *ext2_find_entry(fs_t *fs, inode_t *inode, const char *name)
{
  if ((inode->type & 0xf000) != INODE_TYPE_DIRECTORY) {
    return 0;
  }

  /* traverse directory entries */
  uint16_t name_length = strlen(name);
  uint16_t num_entries = inode->num_hard_links;
  uint16_t i = 0;
  uint16_t block = 0;

  while (i < num_entries) {
    void *dirdata = ext2_read_block(fs, inode->pointer0[block]);
    size_t offset = 0;
    while (offset < fs->block_size) {
      dir_entry_t *entry = dirdata + offset;
      if (entry->size == 0) {
        return 0;
      }
      int eq = (entry->name_length_lo == name_length) &&
               !memcmp(entry->name, name, entry->name_length_lo);

      if (eq) {
        return ext2_get_inode(fs, entry->inode);
      }

      offset += entry->size;
      i++;
    }
    block++;
  }

  return 0;
}

uint32_t ext2_block_size(superblock_t *sb)
{
  return 1024 << sb->log_block_size;
}

uint16_t ext2_inode_size(superblock_t *sb)
{
  if (sb->version_major >= 1) {
    return sb->inode_size;
  } else {
    return 128;
  }
}

inode_iterator_t *ext2_inode_iterator_new(fs_t *fs, inode_t *inode)
{
  inode_iterator_t *it = MALLOC(sizeof(inode_iterator_t));
  it->fs = fs;
  it->inode = inode;
  it->index = 0;
  return it;
}

void ext2_inode_iterator_del(inode_iterator_t *it)
{
  FREE(it);
}

uint32_t ext2_inode_iterator_datablock(inode_iterator_t *it) {
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

void *ext2_inode_iterator_read(inode_iterator_t *it) {
  uint32_t block = ext2_inode_iterator_datablock(it);
  return ext2_read_block(it->fs, block);
}

/* read into a preallocated buffer, which must be at least as big as
the block size */
void ext2_inode_iterator_read_into(inode_iterator_t *it, void *buffer)
{
  uint32_t block = ext2_inode_iterator_datablock(it);
  return ext2_read_block_into(it->fs, block, buffer);
}

void ext2_inode_iterator_next(inode_iterator_t *it) {
  it->index++;
}

uint32_t ext2_inode_iterator_block_size(inode_iterator_t *it) {
  uint32_t offset = it->index * it->fs->block_size;
  uint32_t ret = it->inode->size_lo - offset;
  if (ret > it->fs->block_size) {
    return it->fs->block_size;
  } else {
    return ret;
  }
}

int ext2_inode_iterator_end(inode_iterator_t *it) {
  return it->index * it->fs->block_size >= it->inode->size_lo;
}

#ifndef __EXT2_H__
#define __EXT2_H__

#include <stddef.h>
#include <stdint.h>

typedef struct {
  uint32_t num_inodes;
  uint32_t num_blocks;
  uint32_t num_reserved_blocks;
  uint32_t num_unalloc_blocks;
  uint32_t num_unalloc_inodes;
  uint32_t superblock_offset;
  uint32_t log_block_size;
  uint32_t log_frag_size;
  uint32_t blocks_per_group;
  uint32_t frags_per_group;
  uint32_t inodes_per_group;
  uint32_t last_mount_time;
  uint32_t last_written_time;
  uint16_t num_mounts;
  uint16_t num_mounts_max;
  uint16_t signature;
  uint16_t state;
  uint16_t error_method;
  uint16_t version_minor;
  uint32_t last_fsck_time;
  uint32_t fsck_interval;
  uint32_t os_id;
  uint32_t version_major;
  uint16_t superuser_id;
  uint16_t supergroup_id;
  uint32_t first_nonreserved_inode;
  uint16_t inode_size;
} __attribute__((packed)) ext2_superblock_t;

typedef struct {
  uint32_t block_bitmap_offset;
  uint32_t inode_bitmap_offset;
  uint32_t inode_table_offset;
  uint16_t num_unalloc_blocks;
  uint16_t num_unalloc_inodes;
  uint16_t num_directories;
  uint8_t unused[14];
} __attribute__((packed)) ext2_group_descriptor_t;

typedef struct ext2_inode {
  uint16_t type;
  uint16_t user_id;
  uint32_t size_lo;
  uint32_t atime;
  uint32_t ctime;
  uint32_t mtime;
  uint32_t dtime;
  uint16_t group_id;
  uint16_t num_hard_links;
  uint32_t num_sectors;
  uint32_t flags;
  uint32_t os_value;
  uint32_t pointer0[12];
  uint32_t pointer1;
  uint32_t pointer2;
  uint32_t pointer3;
  uint32_t generation;
} __attribute__((packed)) ext2_inode_t;

typedef struct {
  uint32_t inode;
  uint16_t size;
  uint8_t name_length_lo;
  uint8_t type;
  char name[];
} __attribute__((packed)) ext2_dir_entry_t;

typedef struct fs_struct fs_t;

enum
{
  INODE_TYPE_FIFO = 0x1000,
  INODE_TYPE_CDEV = 0x2000,
  INODE_TYPE_DIRECTORY = 0x4000,
  INODE_TYPE_FILE = 0x8000,
  INODE_TYPE_LINK = 0xa000,
  INODE_TYPE_SOCKET = 0xc000
};


void* ext2_read_block(fs_t *fs, unsigned int offset);
uint32_t ext2_block_size(ext2_superblock_t *sb);
uint16_t ext2_inode_size(ext2_superblock_t *sb);

/* fs_t structure */

struct storage;
fs_t *ext2_new_fs(struct storage *storage);
void ext2_free_fs(fs_t *fs);
size_t ext2_fs_block_size(fs_t *fs);

/* inode API */
/* note that the returned pointers are invalidated by further API calls */

ext2_inode_t *ext2_get_inode(fs_t* fs, unsigned int i);
ext2_inode_t *ext2_get_path_inode(fs_t *fs, const char *path);
ext2_inode_t *ext2_find_entry(fs_t *fs, ext2_inode_t *inode, const char *name);

/* iterator API */
typedef struct {
  fs_t *fs;
  ext2_inode_t *inode;
  uint32_t index;
} inode_iterator_t;

void ext2_inode_iterator_init(inode_iterator_t *it, fs_t *fs, ext2_inode_t *inode);
inode_iterator_t *ext2_inode_iterator_new(fs_t *fs, ext2_inode_t *inode);
void ext2_inode_iterator_del(inode_iterator_t *it);
uint32_t ext2_inode_iterator_datablock(inode_iterator_t *it);
void *ext2_inode_iterator_read(inode_iterator_t *it);
void ext2_inode_iterator_read_into(inode_iterator_t *it, void *buffer);
void ext2_inode_iterator_next(inode_iterator_t *it);
void ext2_inode_iterator_set_index(inode_iterator_t *it, int index);
int ext2_inode_iterator_index(inode_iterator_t *it);
uint32_t ext2_inode_iterator_block_size(inode_iterator_t *it);
int ext2_inode_iterator_end(inode_iterator_t *it);

#if _HELIUM
# if _HELIUM_LOADER
void *loader_kmalloc(size_t sz);
void loader_kfree(void *p);
#  define MALLOC loader_kmalloc
#  define FREE loader_kfree
# else
#  include "kernel/kmalloc.h"
#  define MALLOC kmalloc
#  define FREE kfree
# endif /* _HELIUM_LOADER */
#else
# include <stdlib.h>
# define MALLOC malloc
# define FREE free
#endif /* _HELIUM */

#endif /* __EXT2_H__ */

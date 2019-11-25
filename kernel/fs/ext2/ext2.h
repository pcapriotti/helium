#ifndef __EXT2_H__
#define __EXT2_H__

#include <stddef.h>
#include <stdint.h>

struct storage;
struct allocator;
struct storage_mapping;

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
} __attribute__((packed)) ext2_gdesc_t;

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
  uint32_t file_acl;
  uint32_t size_hi;
  uint32_t faddr;
  unsigned char os_value2[12];
} __attribute__((packed)) ext2_inode_t;

typedef struct {
  uint32_t inode;
  uint16_t size;
  uint8_t name_length_lo;
  uint8_t type;
  char name[];
} __attribute__((packed)) ext2_dir_entry_t;

typedef struct ext2 {
  struct storage *storage;
  struct allocator *allocator;
  struct storage_mapping *sb_map;
  struct storage_mapping *gdesc_map;
  unsigned char *buf; /* must be at least as big as the block size */
  size_t block_size;
  size_t inode_size;
} ext2_t;

enum
{
  INODE_TYPE_FIFO = 0x1000,
  INODE_TYPE_CDEV = 0x2000,
  INODE_TYPE_DIRECTORY = 0x4000,
  INODE_TYPE_FILE = 0x8000,
  INODE_TYPE_LINK = 0xa000,
  INODE_TYPE_SOCKET = 0xc000
};


void* ext2_read_block(ext2_t *fs, unsigned int offset);
void ext2_read_block_into(ext2_t *fs, unsigned int offset, void *buffer);
uint32_t ext2_block_size(ext2_superblock_t *sb);
void ext2_write(ext2_t *fs, unsigned offset, void *buffer,
                void *x, size_t size);

/* ext2_t structure */

struct storage;
struct allocator;

ext2_t *ext2_new_fs(struct storage *storage, struct allocator *allocator);
void ext2_free_fs(ext2_t *fs);
size_t ext2_fs_block_size(ext2_t *fs);

/* inode API */
/* note that the returned pointers are invalidated by further API calls */

ext2_inode_t *ext2_get_inode(ext2_t* fs, unsigned int i);
ext2_inode_t *ext2_get_path_inode(ext2_t *fs, const char *path);
ext2_inode_t *ext2_find_entry(ext2_t *fs, ext2_inode_t *inode, const char *name);
int ext2_get_free_inode(ext2_t *fs, unsigned group);
ext2_inode_t *ext2_create(ext2_t *fs, const char *path);

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

#endif /* __EXT2_H__ */

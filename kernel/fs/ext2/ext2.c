#include "bitset.h"
#include "fs/ext2/ext2.h"
#include "core/allocator.h"
#include "core/mapping.h"
#include "core/storage.h"
#include "core/util.h"

#include <assert.h>
#include <stddef.h>

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

void ext2_read_block_into(ext2_t *fs, unsigned int offset, void *buffer)
{
  storage_read(fs->storage,
               buffer,
               fs->block_size * offset,
               fs->block_size);
}

/*
  Write data at the given offset, plus padding to fit underlying block
  size. The area pointed by x of length size must be within the
  buffer.
 */
void ext2_write(ext2_t *fs, unsigned offset, void *buffer,
                void *x, size_t size)
{
  size_t loc_offset = x - buffer;
  assert(loc_offset < fs->block_size);

  const int sector_size = storage_sector_size(fs->storage);
  unsigned start = loc_offset / sector_size;
  unsigned end = DIV_UP(loc_offset + size, sector_size);

  storage_write(fs->storage,
                buffer + start,
                fs->block_size * offset + start,
                end - start);
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

static ext2_superblock_t *ext2_superblock(ext2_t *fs)
{
  return storage_mapping_read_item(fs->sb_map, 0, ext2_superblock_t);
}

static ext2_gdesc_t *ext2_gdesc(ext2_t *fs, unsigned group)
{
  return storage_mapping_read_item(fs->gdesc_map, group, ext2_gdesc_t);
}

static uint16_t ext2_superblock_inode_size(ext2_superblock_t *sb)
{
  if (sb->version_major >= 1) {
    return sb->inode_size;
  } else {
    return 128;
  }
}

ext2_t *ext2_new_fs(storage_t *storage, allocator_t *allocator)
{
#if EXT2_DEBUG
  TRACE("[ext2] locating superblock\n");
#endif

  storage_mapping_t *sb_map = storage_mapping_new
    (allocator, storage, 1024, (sizeof(ext2_superblock_t)));
  ext2_superblock_t *sb = storage_mapping_read_item
    (sb_map, 0, ext2_superblock_t);
  if (sb->signature != 0xef53) {
#if EXT2_DEBUG
    TRACE("[ext2] could not find superblock\n");
#endif
    storage_mapping_del(sb_map, allocator);
    return 0;
  }

  ext2_t *fs = allocator_alloc(allocator, sizeof(ext2_t));
  fs->storage = storage;
  fs->allocator = allocator;
  fs->sb_map = sb_map;
  fs->block_size = ext2_block_size(sb);
  fs->inode_size = ext2_superblock_inode_size(sb);
  fs->buf = allocator_alloc(fs->allocator, fs->block_size);

  /* cache block group descriptor table */
  int num_groups = DIV_UP(sb->num_blocks, sb->blocks_per_group);
  size_t gdesc_offset = (ext2_superblock(fs)->superblock_offset + 1)
    * fs->block_size;
  size_t gdesc_size = num_groups * sizeof(ext2_gdesc_t);
  fs->gdesc_map = storage_mapping_new
    (allocator, storage, gdesc_offset,
     ALIGN_UP(gdesc_size, storage_sector_size(storage)));

#if EXT2_DEBUG
  TRACE("block size: %#lx inode size: %#lx superblock offset: %#x\n",
        fs->block_size, fs->inode_size,
        ext2_superblock(fs)->superblock_offset);
#endif

  return fs;
}

void ext2_free_fs(ext2_t *fs) {
  allocator_free(fs->allocator, fs->buf);
  allocator_free(fs->allocator, fs);
}

int ext2_num_bgroups(ext2_superblock_t *sb) {
  if (sb->blocks_per_group == 0) {
    return -1;
  }
  int n1 = DIV_UP(sb->num_blocks, sb->blocks_per_group);
  if (sb->inodes_per_group == 0) {
    return -2;
  }
  int n2 = DIV_UP(sb->num_inodes, sb->inodes_per_group);
  if (n1 != n2) {
    return -3;
  }
  return n1;
}

static int find_in_bitmap(uint32_t *bitmap, size_t size)
{
  for (unsigned i = 0; i < size; i++) {
    uint32_t x = ~bitmap[i];
    if (x) return __builtin_ctz(x) + (i << 5);
  }
  return -1;
}

static int find_in_mapped_bitmap(storage_mapping_t *map, size_t size)
{
  const int num_chunks = DIV_UP(size, map->buf_size);
  for (int i = 0; i < num_chunks; i++) {
    uint32_t *bitmap = storage_mapping_read
      (map, i * map->buf_size, map->buf_size);
    size_t len = map->buf_size / sizeof(uint32_t);
    int index = find_in_bitmap(bitmap, len);
    if (index != -1) {
      SET_BIT(bitmap, index);
      storage_mapping_put
        (map, &BIT_WORD(bitmap, index), sizeof(uint32_t));
      return index;
    }
  }

  return -1;
}

static void mapped_bitmap_unset(storage_mapping_t *map, int index)
{
  uint32_t *word = storage_mapping_read_item(map, index >> 5, uint32_t);
  *word &= ~(1UL << (index & 0x1f));
  storage_mapping_put(map, word, sizeof(uint32_t));
}

int ext2_get_free_inode(ext2_t *fs, unsigned group)
{
  ext2_gdesc_t *desc = ext2_gdesc(fs, group);
  if (desc->num_unalloc_inodes == 0) return -1;

  storage_mapping_t bitmap;
  storage_mapping_init(&bitmap, fs->storage,
                       desc->inode_bitmap_offset * fs->block_size,
                       fs->buf,
                       fs->block_size);

  const size_t bitmap_size = ext2_superblock(fs)->inodes_per_group >> 3;
  int index = find_in_mapped_bitmap(&bitmap, bitmap_size);
  if (index != -1) {
    desc->num_unalloc_inodes--;
    storage_mapping_put(fs->gdesc_map, desc,
                        sizeof(ext2_gdesc_t));
  }

  return index;
}

typedef struct ext2_indexed_inode {
  ext2_inode_t *inode;
  unsigned index;
} ext2_indexed_inode_t;

ext2_indexed_inode_t ext2_new_inode(ext2_t *fs, unsigned group, uint16_t type)
{
  TRACE("inode size: %lu\n", fs->inode_size);
  const int inodes_per_table_block = fs->block_size / fs->inode_size;
  TRACE("inode num: %d\n", inodes_per_table_block);

  /* get a fresh local inode index */
  int index = ext2_get_free_inode(fs, group);
  if (index == -1) return (ext2_indexed_inode_t) { 0, 0 };
  TRACE("inode loc index: %d\n", index);

  /* locate inode in the table */
  size_t inode_table_offset = ext2_gdesc(fs, group)->inode_bitmap_offset;
  inode_table_offset += index / inodes_per_table_block;
  ext2_read_block(fs, inode_table_offset);
  ext2_inode_t *table = (ext2_inode_t *) fs->buf;
  ext2_inode_t *inode = &table[index % inodes_per_table_block];
  TRACE("inode block index: %lu\n", (inode - table));
  TRACE("loc offset: %lu\n", (unsigned char *)inode - fs->buf);

  /* fill inode structure */
  memset(inode, 0, fs->inode_size);
  inode->type = type;

  /* save inode in the table */
  ext2_write(fs, inode_table_offset, fs->buf,
             inode, fs->inode_size);

  unsigned idx = group * ext2_superblock(fs)->inodes_per_group + index + 1;
  return (ext2_indexed_inode_t) { inode, idx };
}

void ext2_del_block(ext2_t *fs, unsigned block)
{
  unsigned blocks_per_group = ext2_superblock(fs)->blocks_per_group;
  unsigned group = block / blocks_per_group;
  ext2_gdesc_t *desc = ext2_gdesc(fs, group);
  desc->num_unalloc_blocks++;

  size_t block_bitmap_offset = desc->block_bitmap_offset;
  storage_mapping_t bitmap;
  storage_mapping_init(&bitmap, fs->storage,
                       desc->block_bitmap_offset * fs->block_size,
                       fs->buf,
                       fs->block_size);
  mapped_bitmap_unset(&bitmap, block % blocks_per_group);
  storage_mapping_put(fs->gdesc_map, desc, sizeof(ext2_gdesc_t));
}

unsigned ext2_new_block(ext2_t *fs, unsigned group)
{
  ext2_gdesc_t *desc = ext2_gdesc(fs, group);
  if (desc->num_unalloc_blocks == 0) return -1;

  size_t block_bitmap_offset = desc->block_bitmap_offset;
  storage_mapping_t bitmap;
  storage_mapping_init(&bitmap, fs->storage,
                       desc->block_bitmap_offset * fs->block_size,
                       fs->buf,
                       fs->block_size);

  const size_t bitmap_size = ext2_superblock(fs)->blocks_per_group >> 3;
  int index = find_in_mapped_bitmap(&bitmap, bitmap_size);
  if (index != -1) {
    desc->num_unalloc_blocks--;
    storage_mapping_put(fs->gdesc_map, desc, sizeof(ext2_gdesc_t));
  }

  return index;
}

static ext2_dir_entry_t *ext2_new_dir_entry(ext2_t *fs,
                                            unsigned group,
                                            ext2_inode_t *dir,
                                            const char *name,
                                            size_t name_len)
{
  ext2_dir_iterator_t it;
  if (ext2_dir_iterator_init(&it, fs, dir) == -1)
    return 0;

  if (name_len > 255) return 0;

  ext2_dir_entry_t *entry;

  while ((entry = ext2_dir_iterator_next(&it))) {
    /* deleted entry, reuse */
    if (!entry->inode && entry->size >= name_len + sizeof(ext2_dir_entry_t)) {
      return entry;
    }

    /* check whether there is enough free space in the padding */
    void *entry1 = (void *)entry + entry->size;
    void *entry_end = (void *)entry->name + entry->name_length_lo;
    entry_end = (void *) ALIGN_UP_BITS((size_t) entry_end, 2);
    if (entry_end + sizeof(ext2_dir_entry_t) + name_len <= entry1) {
      return entry_end;
    }
  }

  /* no space for an entry, allocate a new block */
  unsigned index = ext2_new_block(fs, group);
  assert(!"not implemented");
  return 0;
}

ext2_inode_t *ext2_get_inode(ext2_t* fs, unsigned int index)
{
  index--;
  unsigned int group = index / ext2_superblock(fs)->inodes_per_group;
  ext2_gdesc_t *gdesc = ext2_gdesc(fs, group);

  unsigned int index_in_group = index %
    ext2_superblock(fs)->inodes_per_group;
  unsigned int inodes_per_block = fs->block_size / fs->inode_size;
  unsigned int block = index_in_group / inodes_per_block;

  void *table = ext2_read_block(fs, gdesc->inode_table_offset + block);
  unsigned int index_in_block = index_in_group % inodes_per_block;
  return table + fs->inode_size * index_in_block;
}

uint64_t ext2_inode_size(ext2_inode_t *inode)
{
  return (uint64_t) inode->size_lo |
    ((uint64_t) inode->size_hi << 32);
}

void ext2_inode_set_size(ext2_inode_t *inode, uint64_t size)
{
  inode->size_lo = size & 0xffffffff;
  inode->size_hi = size >> 32;
}

uint32_t *ext2_inode_block_pointer(ext2_t *fs,
                                   ext2_inode_t *inode,
                                   uint32_t index)
{
  uint32_t index1, index2, index3;

  /* level 0 */
  if (index < 12) {
    return inode->pointer0 + index;
  }
  uint32_t pointers_per_block = fs->block_size / sizeof(uint32_t);

  /* level 1 */
  index -= 12;
  if (index < pointers_per_block) {
    uint32_t *pointer1 = ext2_read_block(fs, inode->pointer1);
    return &pointer1[index];
  }

  /* level 2 */
  index -= pointers_per_block;
  index2 = index / pointers_per_block;
  index1 = index % pointers_per_block;
  if (index2 < pointers_per_block) {
    uint32_t *pointer2 = ext2_read_block(fs, inode->pointer2);
    uint32_t *pointer1 = ext2_read_block(fs, pointer2[index2]);
    return &pointer1[index1];
  }

  /* level 3 */
  index -= pointers_per_block * pointers_per_block;
  index1 = index % pointers_per_block;
  index /= pointers_per_block;
  index2 = index % pointers_per_block;
  index3 = index / pointers_per_block;
  if (index3 < pointers_per_block) {
    uint32_t *pointer3 = ext2_read_block(fs, inode->pointer3);
    uint32_t *pointer2 = ext2_read_block(fs, pointer3[index3]);
    uint32_t *pointer1 = ext2_read_block(fs, pointer2[index2]);
    return &pointer1[index1];
  }

  return 0;
}

void ext2_inode_set_block(ext2_t *fs, ext2_inode_t *inode,
                          unsigned index, unsigned block)
{
  *ext2_inode_block_pointer(fs, inode, index) = block;
}

void ext2_inode_del_block(ext2_t *fs, ext2_inode_t *inode,
                          unsigned index)
{
  uint32_t block = *ext2_inode_block_pointer(fs, inode, index);
  ext2_del_block(fs, block);
}

typedef struct path_iterator {
  char *path;
  char *saveptr;
  char *token;
} path_iterator_t;

void path_iterator_init(path_iterator_t *it,
                        allocator_t *allocator,
                        const char *path)
{
  it->path = allocator_alloc(allocator, strlen(path) + 1);
  strcpy(it->path, path);

  it->saveptr = 0;
  it->token = strtok_r(it->path, "/", &it->saveptr);
}

char *path_iterator_next(path_iterator_t *it)
{
  if (!it->token) return 0;

  char *token = it->token;
  it->token = strtok_r(0, "/", &it->saveptr);
  return token;
}

void path_iterator_cleanup(path_iterator_t *it,
                           allocator_t *allocator)
{
  allocator_free(allocator, it->path);
}

static inline ext2_inode_t *ext2_root(ext2_t *fs)
{
  return ext2_get_inode(fs, 2);
}

ext2_inode_t *ext2_get_path_inode(ext2_t *fs, const char *path)
{
  /* get root first */
  ext2_inode_t *inode = ext2_root(fs);
  if (!inode) return 0;

  path_iterator_t path_it;
  path_iterator_init(&path_it, fs->allocator, path);

  char *token;
  while (inode && (token = path_iterator_next(&path_it))) {
    ext2_inode_t parent = *inode;
    inode = ext2_find_entry(fs, &parent, token);
    token = path_iterator_next(&path_it);
  }
  path_iterator_cleanup(&path_it, fs->allocator);
  return inode;
}

ext2_inode_t *ext2_create(ext2_t *fs, const char *path)
{
  ext2_inode_t *parent = ext2_root(fs);
  if (!parent) return 0;

  path_iterator_t path_it;
  path_iterator_init(&path_it, fs->allocator, path);

  char *token0 = path_iterator_next(&path_it);
  if (!token0) return 0;

  char *token;
  while (parent && (token = path_iterator_next(&path_it))) {
    ext2_inode_t p = *parent;
    parent = ext2_find_entry(fs, &p, token0);
    token0 = token;
    token = path_iterator_next(&path_it);
  }
  path_iterator_cleanup(&path_it, fs->allocator);
  if (!parent) return 0;

  const int group = 0;
  ext2_indexed_inode_t ret = ext2_new_inode(fs, group, INODE_TYPE_FILE);
  if (!ret.inode) return 0;

  const size_t len = strlen(token);
  ext2_dir_entry_t *entry = ext2_new_dir_entry
    (fs, group, parent, token, len);
  if (!entry) return 0;

  entry->inode = ret.index;
  entry->size = 0;
  entry->name_length_lo = len;
  entry->type = 2; /* TODO: use correct type */
  memcpy(entry->name, token, len);

  return ret.inode;
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
    if (entry->inode &&
        entry->name_length_lo == name_length &&
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

int ext2_inode_resize(ext2_t *fs, ext2_inode_t *inode, uint64_t size)
{
  uint64_t old_size = ext2_inode_size(inode);
  ext2_inode_set_size(inode, size);

  uint32_t num_blocks = inode->num_sectors /
    (fs->block_size / storage_sector_size(fs->storage));
  inode->num_sectors = DIV64_UP(size, storage_sector_size(fs->storage));

  /* TODO pass correct group */
  const unsigned group = 0;

  if (size > old_size) {
    /* reserve new blocks */
    while (size > old_size) {
      unsigned block = ext2_new_block(fs, group);
      ext2_inode_set_block(fs, inode, num_blocks++, block);
      old_size += fs->block_size;
    }
  }
  else {
    /* reclaim blocks */
    while (old_size > size) {
      unsigned block = --num_blocks;
      ext2_inode_del_block(fs, inode, block);
      old_size -= fs->block_size;
    }
  }

  return 0;
}

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

int ext2_dir_iterator_init(ext2_dir_iterator_t *it,
                           ext2_t *fs, ext2_inode_t *inode)
{
  if ((inode->type & 0xf000) != INODE_TYPE_DIRECTORY) {
    return -1;
  }

  it->fs = fs;
  it->index = 0;
  it->block = 0;
  it->block_offset = 0;
  ext2_inode_iterator_init(&it->inode_it, fs, inode);

  return 0;
}

int ext2_inode_iterator_resize(ext2_inode_iterator_t *it, uint64_t size)
{
  return ext2_inode_resize(it->fs, &it->inode, size);
}

void ext2_dir_iterator_cleanup(ext2_dir_iterator_t *it)
{
  allocator_free(it->fs->allocator, it->block);
}

ext2_dir_entry_t *ext2_dir_iterator_next(ext2_dir_iterator_t *it)
{
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

  it->block_offset += entry->size;
  it->index++;

  return entry;
}

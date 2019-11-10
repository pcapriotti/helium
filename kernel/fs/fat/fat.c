#include "core/allocator.h"
#include "core/debug.h"
#include "core/storage.h"
#include "fs/fat/fat.h"
#include "memory.h"

#include <assert.h>
#include <string.h>

unsigned long fat_total_sectors(fat_superblock_t *sb)
{
  if (sb->total_sectors_16)
    return sb->total_sectors_16;
  else
    return sb->total_sectors_32;
}

unsigned long fat_sectors_per_fat(fat_superblock_t *sb)
{
  if (sb->sectors_per_fat_16)
    return sb->sectors_per_fat_16;
  else
    return sb->v7.sectors_per_fat_32;
}

static int fat_match_signature(fat_bpb_v4_t *v4)
{
  if (v4->signature != 0x28 && v4->signature != 0x29)
    return FAT_VERSION_INVALID;

  if (v4->fs_type[0] != 'F') return FAT_VERSION_INVALID;
  if (v4->fs_type[1] != 'A') return FAT_VERSION_INVALID;
  if (v4->fs_type[2] != 'T') return FAT_VERSION_INVALID;

  if (!memcmp(&v4->fs_type[3], "12   ", 5)) return FAT_VERSION_FAT12;
  if (!memcmp(&v4->fs_type[3], "16   ", 5)) return FAT_VERSION_FAT16;
  if (!memcmp(&v4->fs_type[3], "32   ", 5)) return FAT_VERSION_FAT32;
  if (!memcmp(&v4->fs_type[3], "     ", 5)) return FAT_VERSION_UNKNOWN;

  return FAT_VERSION_INVALID;
}

static int fat_declared_version(fat_superblock_t *sb)
{
  int version = fat_match_signature(&sb->v7.v4);
  if (version == FAT_VERSION_INVALID) {
    version = fat_match_signature(&sb->v4);
  }
  /* TODO check for a BPB 3.4 */
  return version;
}

static int fat_version(fat_t *fat, fat_superblock_t *sb)
{
  int version = fat_declared_version(sb);
  if (version != FAT_VERSION_UNKNOWN)
    return version;

  if (fat->num_clusters <= 4086) return FAT_VERSION_FAT12;
  if (fat->num_clusters <= 65526) return FAT_VERSION_FAT16;

  return FAT_VERSION_FAT32;
}

size_t fat_cluster_offset(fat_t *fat, unsigned cluster)
{
  assert(cluster >= 2);
  return fat->data_offset + (cluster - 2) * fat->cluster_size;
}

unsigned fat_map_12_next(fat_t *fat, unsigned cluster)
{
  uint16_t *entry = fat->map + cluster + cluster / 2;
  return (*entry >> (4 * (cluster & 1))) & 0xfff;
}

unsigned fat_map_16_next(fat_t *fat, unsigned cluster)
{
  const uint16_t *map = fat->map;
  return map[cluster];
}

unsigned fat_map_32_next(fat_t *fat, unsigned cluster)
{
  const uint32_t *map = fat->map;
  return map[cluster] & 0xffffff;
}

void fat_init(fat_t *fat, storage_t *storage, allocator_t *allocator)
{
  /* read superblock */
  fat->buffer = allocator_alloc(allocator, 1 << storage->ops->alignment);
  storage_read(storage, fat->buffer, 0, 1 << storage->ops->alignment);
  fat_superblock_t *sb = fat->buffer;

  /* determine width */

  /* save superblock info */
  fat->cluster_size = sb->bytes_per_sector * sb->sectors_per_cluster;
  fat->map_offset = sb->bytes_per_sector * sb->num_reserved_sectors;
  fat->map_size = sb->bytes_per_sector * fat_sectors_per_fat(sb);
  fat->dir_offset = fat->map_offset + fat->map_size * sb->num_fats;
  size_t dir_size = sb->num_root_dir_entries * sizeof(fat_dir_entry_t);
  fat->data_offset = fat->dir_offset +
    (dir_size - dir_size % sb->bytes_per_sector);
  fat->num_clusters = (fat_total_sectors(sb) * sb->bytes_per_sector - fat->data_offset) /
    fat->cluster_size;
  fat->version = fat_version(fat, sb);
#if FAT_DEBUG
  serial_printf("[fat] version: %d, num_clusters: %u, cluster_size: %u\n",
                fat->version, fat->num_clusters, fat->cluster_size);
  serial_printf("[fat] map_offset: %#x, map_size: %u, data_offset: %#x\n",
                fat->map_offset, fat->map_size, fat->data_offset);
  serial_printf("[fat] dir_offset: %#x, dir_entries: %u\n",
                fat->dir_offset, sb->num_root_dir_entries);
#endif
  allocator_free(allocator, fat->buffer);

  /* copy the FAT map to memory */
  fat->map = allocator_alloc(allocator, fat->map_size);
  storage_read(storage, fat->map, fat->map_offset, fat->map_size);
  switch (fat->version) {
  case FAT_VERSION_FAT12:
    fat->next = fat_map_12_next;
    break;
  case FAT_VERSION_FAT16:
    fat->next = fat_map_16_next;
    break;
  case FAT_VERSION_FAT32:
    fat->next = fat_map_32_next;
    break;
  }

  /* allocate a cluster-size buffer */
  fat->buffer = allocator_alloc(allocator, fat->cluster_size);
  fat->allocator = allocator;
  fat->storage = storage;
}

unsigned fat_map_next(fat_t *fat, unsigned cluster)
{
  return fat->next(fat, cluster);
}

int fat_end_of_chain(fat_t *fat, unsigned cluster)
{
  if (cluster < 2) return 1;
  switch (fat->version) {
  case FAT_VERSION_FAT32:
    return cluster >= 0xffffff8;
  case FAT_VERSION_FAT16:
    return cluster >= 0xfff8;
  case FAT_VERSION_FAT12:
    return cluster >= 0xff8;
  }
  return 1;
}

void fat_cleanup(fat_t *fat)
{
  allocator_free(fat->allocator, fat->buffer);
}

int fat_read_cluster(fat_t *fat, void *buffer, unsigned cluster)
{
  assert(cluster >= 2 && cluster < fat->num_clusters + 2);
  unsigned index = cluster - 2;

  size_t offset = fat->data_offset + index * fat->cluster_size;
#ifdef FAT_DEBUG
  serial_printf("[fat] reading cluster %u from offset %#x\n",
                cluster, offset);
#endif
  return storage_read(fat->storage, buffer,
                      offset, fat->cluster_size);
}

int fat_read_root_dir_cluster(fat_t *fat, void *buffer, size_t offset)
{
  offset += fat->dir_offset;
  return storage_read(fat->storage, buffer,
                      offset, fat->cluster_size);
}

int fat_dir_iterator_init(fat_dir_iterator_t *it, fat_t *fs, unsigned cluster)
{
  it->fs = fs;
  it->cluster = cluster;
  it->offset = 0;
  it->index = 0;
  it->buffer = 0;
  it->eof = 0;

  return 0;
}

void fat_dir_iterator_cleanup(fat_dir_iterator_t *it)
{
  allocator_free(it->fs->allocator, it->buffer);
}

fat_dir_entry_t *fat_dir_iterator_next(fat_dir_iterator_t *it)
{
  if (it->eof) return 0;

  /* if we are on a cluster boundary */
  if (it->offset == 0) {
    if (!it->buffer) {
      it->buffer = allocator_alloc(it->fs->allocator, it->fs->cluster_size);
    }
    if (it->cluster >= 2) {
      /* read data cluster */
      fat_read_cluster(it->fs, it->buffer, it->cluster);
    }
    else {
      /* read root directory cluster */
      fat_read_root_dir_cluster(it->fs, it->buffer,
                                it->index * sizeof(fat_dir_entry_t));
    }
  }

  fat_dir_entry_t *entry = it->buffer + it->offset;

  it->index++;
  it->offset += sizeof(fat_dir_entry_t);
  if (it->offset >= it->fs->cluster_size) {
    it->offset = 0;
    if (it->cluster >= 2) {
      it->cluster = fat_map_next(it->fs, it->cluster);
      if (fat_end_of_chain(it->fs, it->cluster))
        it->eof = 1;
    }
  }

  /* no more entries */
  if (entry->filename[0] == '\0') {
    it->eof = 1;
    return 0;
  }

  return entry;
}

static unsigned fat_entry_cluster(fat_t *fs,
                                  fat_dir_entry_t *entry)
{
  switch (fs->version) {
  case FAT_VERSION_FAT32:
    return entry->cluster_lo |
      ((unsigned) entry->cluster_hi << 16);
  default:
    return entry->cluster_lo;
  }
}

static char tolower(char c)
{
  if (c >= 'A' && c <= 'Z')
    return 'a' + (c - 'A');
  else
    return c;
}

static char fat_entry_filename_char(fat_dir_entry_t *entry, int index)
{
  assert(index >= 0 && index < 11);
  char c = entry->filename[index];
  if ((index < 8 && entry->case_flags && VFAT_LOWER_BASE)
      || (index >= 8 && entry->case_flags && VFAT_LOWER_EXT)) {
    return tolower(c);
  }

  return c;
}

static int fat_entry_filename_eq(fat_dir_entry_t *entry,
                                 const char *filename,
                                 size_t len)
{
  assert(len <= 11);

#if FAT_DEBUG
  {
    serial_printf("found entry: \n");
    for (int i = 0; i < 11; i++) {
      serial_printf("%c", fat_entry_filename_char(entry, i));
    }
    serial_printf("\n");
  }
#endif

  /* deleted entries */
  if (entry->filename[0] == (char) 0xe5) return 0;

  /* special first character */
  if (entry->filename[0] == 0x05)
    entry->filename[0] = 0xe5;

  for (int i = 0; i < (int) len; i++) {
    if (fat_entry_filename_char(entry, i) != filename[i])
      return 0;
  }

  for (int i = len; i < 11; i++) {
    if (entry->filename[i] != ' ') return 0;
  }

  return 1;
}

static fat_dir_entry_t *fat_find_entry(fat_t *fs,
                                       unsigned cluster,
                                       const char *filename)
{
  size_t len = strlen(filename);
  if (len > 11) return 0;

  fat_dir_iterator_t it;
  if (fat_dir_iterator_init(&it, fs, cluster) == -1)
    return 0;

  fat_dir_entry_t *entry;
  while ((entry = fat_dir_iterator_next(&it))) {
    if (fat_entry_filename_eq(entry, filename, len)) {
      fat_dir_iterator_cleanup(&it);
      return entry;
    }
  }

  fat_dir_iterator_cleanup(&it);
  return 0;
}

int fat_path_cluster(fat_t *fs, const char *path, unsigned *result)
{
  int path_len = strlen(path);
  char *pbuf = allocator_alloc(fs->allocator, path_len + 1);
  strcpy(pbuf, path);

  char *saveptr = 0;
  char *token = strtok_r(pbuf, "/", &saveptr);

  unsigned cluster = 0;

  if (token == 0) { /* root */
    allocator_free(fs->allocator, pbuf);
    return cluster;
  }

  while (token) {
    fat_dir_entry_t *entry = fat_find_entry(fs, cluster, token);
    if (entry) {
      cluster = fat_entry_cluster(fs, entry);
      token = strtok_r(0, "/", &saveptr);
    }
    else {
      allocator_free(fs->allocator, pbuf);
      return -1;
    }
  }

  allocator_free(fs->allocator, pbuf);
  *result = cluster;
  return 0;
}
